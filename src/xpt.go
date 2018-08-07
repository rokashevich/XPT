package main

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"math"
	"net/http"
	"os"
	"os/user"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
)

func main() {
	// Согласно нашей структуре каталогов xpt лежит в sandbox/etc/xpt.
	sandbox, _ := filepath.Abs(filepath.Join(filepath.Dir(os.Args[0]), "..", ".."))

	// Настраиваем cache-директорию.
	cache := setupCacheDirectory(sandbox)

	//
	// Обрабатываем аргументы командной строки.
	//

	if len(os.Args) == 2 && os.Args[1] == "update" {
		os.Exit(update(sandbox))
	} else if len(os.Args) > 2 && os.Args[1] == "install" {
		os.Exit(install(sandbox, cache))
	} else {
		os.Exit(usage())
	}
}

func setupCacheDirectory(sandbox string) string {
	cache := os.Getenv("XPTCACHE")
	if cache == "" { // Если XPTCACHE не задан, то используем домашний каталог.
		u, _ := user.Current()
		cache = filepath.Join(u.HomeDir, "xptcache")
	}
	if _, err := os.Stat(cache); os.IsNotExist(err) {
		os.MkdirAll(cache, os.ModePerm)
	}
	return cache
}

func update(sandbox string) int {
	sourcesTxt := filepath.Join(sandbox, "etc", "xpt", "sources.txt")
	dat, err := ioutil.ReadFile(sourcesTxt)
	if err != nil {
		fmt.Println("*** Error: sources.txt not found: " + sourcesTxt)
		os.Exit(1)
	}

	updateTxtContent := ""

	updateOne := func(url string, tag string) string {
		repoURL := url
		if tag != "notag" {
			repoURL += "/" + tag
		}
		packagesTxtURL := repoURL + "/packages.txt"
		packagesTxt, err := readUrl(packagesTxtURL)
		if err != nil {
			fmt.Println("!!! Warning: ")
		}
		updateTxtContentPart := ""
		for _, line := range strings.Split(packagesTxt, "\n") {
			packageFileName := stripCtlAndExtFromUTF8(line)
			if line == "" {
				continue
			}
			packageURL := repoURL + "/" + packageFileName
			packageName := strings.SplitN(packageFileName, "_", 2)[0]
			updateTxtContentPart += tag + " " + packageName + " " + packageURL + "\n"
		}
		return updateTxtContentPart
	}
	for _, line := range strings.Split(string(dat), "\n") {
		line = stripCtlAndExtFromUTF8(line)
		if strings.HasPrefix(line, "repo ") {
			// Удаляем двойные пробелы, т.к. строка может быть отформатирована разным кол-вом пробелов для наглядности sources.txt.
			re := regexp.MustCompile(`[\s\p{Zs}]{2,}`)
			line = re.ReplaceAllString(line, " ")
			words := strings.Split(line, " ") // Массив вида [repo http://url tag1 tag2].
			url := words[1]
			tags := words[2:]
			if len(tags) == 0 { // Репозиторий без тэга.
				updateTxtContent += updateOne(url, "notag")
			} else { // Репозиторий с тегом/тегами.
				for _, tag := range tags {
					updateTxtContent += updateOne(url, tag)
				}
			}
		}
	}

	os.MkdirAll(filepath.Join(sandbox, "var", "xpt"), os.ModePerm)
	f, e := os.Create(filepath.Join(sandbox, "var", "xpt", "update.txt"))
	if e != nil {
		panic(e)
	}
	defer f.Close()
	f.WriteString(updateTxtContent)
	f.Sync()
	return 0
}

func install(sandbox string, cache string) int {
	var names []string // Массив названий пакетов для установки.
	tag := ""          // Тэг этих пакетов.
	for _, arg := range os.Args[2:] {
		if arg == "@" {
			tag = os.Args[len(os.Args)-1]
			break
		} else {
			names = append(names, arg)
		}
	}

	var db [][]string // Считаем из update.txt в виде [[tag1 package1 url1], [tag2 package2 url2]].
	updateTxt := filepath.Join(sandbox, "var", "xpt", "update.txt")
	dat, err := ioutil.ReadFile(updateTxt)
	if err != nil {
		fmt.Println("*** Error: update.txt not found: " + updateTxt)
		os.Exit(1)
	}
	for _, line := range strings.Split(string(dat), "\n") {
		splits := strings.Split(line, " ")
		if len(splits) == 3 {
			db = append(db, splits)
		}
	}

	for _, name := range names {
		installOne(sandbox, cache, name, tag, db)
	}
	return 0
}

func installOne(sandbox string, cache string, name string, tag string, db [][]string) {
	fmt.Printf("%s", name)
	var urls []string
	for _, check := range db {
		if check[0] == tag && check[1] == name {
			urls = append(urls, check[2])
		}
	}
	if len(urls) > 1 {
		fmt.Printf("*** Error: More than one url for a package: %v\n", urls)
		os.Exit(1)
	}
	cached_file_name := strings.Replace(urls[0], "http://", "", -1)
	cached_file_name = strings.Replace(cached_file_name, "/", "~", -1)
	cached_file_name = filepath.Join(cache, cached_file_name)
	_ = downloadUrl(urls[0], cached_file_name)
	fmt.Printf("\n")
}

type xptPackage struct {
	name string
	tag  string
	url  string
	size string
}

func usage() int {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")
	fmt.Println("usage: xpt install package1 package2 @ tag")
	return 1
}

// Функция удаляет все непечатаемые символы из строки.
func stripCtlAndExtFromUTF8(str string) string {
	return strings.Map(func(r rune) rune {
		if r >= 32 && r < 127 {
			return r
		}
		return -1
	}, str)
}

// TODO
func readUrl(url string) (string, error) {
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	buf := bytes.NewBuffer(nil)
	_, err = io.Copy(buf, resp.Body)
	if err != nil {
		return "", err
	}
	return buf.String(), nil
}

//https://golangcode.com/download-a-file-with-progress/
func downloadUrl(url string, filepath string) error {
	// Create the file, but give it a tmp file extension, this means we won't overwrite a
	// file until it's downloaded, but we'll remove the tmp extension once downloaded.
	out, err := os.Create(filepath)
	if err != nil {
		fmt.Println("*** error creating " + filepath)
		return err
	}
	defer out.Close()

	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	// Create our progress reporter and pass it to be used alongside our writer
	counter := &WriteCounter{0, 0}
	_, err = io.Copy(out, io.TeeReader(resp.Body, counter))
	//_, err = io.Copy(out, resp.Body)
	if err != nil {
		return err
	}

	e := math.Floor(math.Log(float64(counter.Total)) / math.Log(1024))
	fmt.Printf("%.1f%cB", float64(counter.Total)/math.Pow(1024, e), " KMGTP"[int(e)])
	return nil
}

// WriteCounter counts the number of bytes written to it. It implements to the io.Writer
// interface and we can pass this into io.TeeReader() which will report progress on each
// write cycle.
type WriteCounter struct {
	Total    uint64
	Previous uint64
}

func (wc *WriteCounter) Write(p []byte) (int, error) {
	n := len(p)
	wc.Total += uint64(n)
	for {
		var scale uint64 = 10000                            // Начальный шаг 10КБ: 0 - 100КБ
		if wc.Previous >= 100000 && wc.Previous < 1000000 { // Шаг 100КБ: 100КБ - 1МБ
			scale = 100000
		} else if wc.Previous >= 1000000 && wc.Previous < 10000000 { // Шаг 1МБ: 1МБ - 10МБ
			scale = 1000000
		} else if wc.Previous >= 10000000 && wc.Previous < 100000000 { // Шаг 10МБ: 10МБ - 100МБ
			scale = 10000000
		} else if wc.Previous >= 100000000 && wc.Previous < 1000000000 { // Шаг 100МБ: 100МБ - 1ГБ
			scale = 100000000
		} else if wc.Previous >= 1000000000 { // Шаг 1Гб: 1Гб и больше
			scale = 1000000000
		}
		wc.Previous += scale
		if wc.Previous < wc.Total {
			//fmt.Printf("scle=%d, prev=%d tot=%d .\n", scale, wc.Previous, wc.Total)
			fmt.Printf(".")
		} else {
			wc.Previous -= scale
			//fmt.Printf("scle=%d, prev=%d tot=%d break\n", scale, wc.Previous, wc.Total)
			break
		}
	}
	return n, nil
}
