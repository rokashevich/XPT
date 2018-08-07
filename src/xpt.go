package main

import (
	"archive/zip"
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
)

func main() {
	sandbox, _ := filepath.Abs(filepath.Join(filepath.Dir(os.Args[0]), "..", "..")) // xpt лежит в sandbox/etc/xpt
	cache := filepath.Join(filepath.VolumeName(sandbox), string(filepath.Separator), "xptcache")

	os.MkdirAll(cache, os.ModePerm)
	os.MkdirAll(filepath.Join(sandbox, "var", "xpt", "installed"), os.ModePerm)

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
	tag := "notag"     // Тэг этих пакетов.
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
	cachedZip := strings.Replace(urls[0], "http://", "", -1)
	cachedZip = strings.Replace(cachedZip, "/", "~", -1)
	cachedZip = filepath.Join(cache, cachedZip)
	cachedUnzipped := strings.Replace(cachedZip, ".zip", "", -1)
	cachedUnzippedContent := filepath.Join(cachedUnzipped, "CONTENT")
	nameWithVersion := strings.Replace(filepath.Base(urls[0]), ".zip", "", -1)
	installedFilename := filepath.Join(sandbox, "var", "xpt", "installed", nameWithVersion+".txt")

	if _, err := os.Stat(installedFilename); err == nil {
		fmt.Printf(" is already installed\n")
		return
	}

	// Если пакет уже установлен то и не надо его устанавливать
	_ = downloadUrl(urls[0], cachedZip)

	fmt.Printf("|unzip")
	files, err := Unzip(cachedZip, cachedUnzipped)
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("|move")
	installedGlob, _ := filepath.Glob(filepath.Join(sandbox, "var", "xpt", "installed", name+"_*.txt"))
	for _, installed := range installedGlob {
		dat, err := ioutil.ReadFile(installed)
		if err == nil {
			for _, line := range strings.Split(string(dat), "\n") {
				if line == "" {
					continue
				}
				os.Remove(filepath.Join(sandbox, line))
			}
			os.Remove(installed)
		}
	}

	installedFile, _ := os.Create(installedFilename)
	defer installedFile.Close()
	for _, file := range files {
		if strings.HasPrefix(file, cachedUnzippedContent) {
			fi, _ := os.Stat(file)
			switch mode := fi.Mode(); {
			case mode.IsRegular():
				rel, _ := filepath.Rel(cachedUnzippedContent, file)
				dest := filepath.Join(sandbox, rel)
				if _, err := os.Stat(dest); err == nil {
					fmt.Printf("\n!!! Overwrite %s", dest)
					os.Remove(dest)
				}
				os.MkdirAll(filepath.Dir(dest), os.ModePerm)
				err = os.Rename(file, dest)
				if err != nil {
					log.Fatal(err)
				}
				installedFile.WriteString(rel + "\n")
			}
		}
	}
	installedFile.Sync()

	dat, e := ioutil.ReadFile(filepath.Join(cachedUnzipped, "control.txt"))
	os.RemoveAll(cachedUnzipped)
	fmt.Printf("\n")
	if e == nil {
		for _, line := range strings.Split(string(dat), "\n") {
			if strings.HasPrefix(line, "Depends:") {
				dependantNames := strings.Split(strings.TrimSpace(strings.SplitN(line, ":", 2)[1]), " ")
				for _, dependantName := range dependantNames {
					installOne(sandbox, cache, dependantName, tag, db)
				}
			}
		}
	}
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
	if _, err := os.Stat(filepath); os.IsNotExist(err) {
		out, err := os.Create(filepath + ".tmp")
		if err != nil {
			fmt.Println("*** error creating " + filepath)
			return err
		}

		// Get the data
		resp, err := http.Get(url)
		if err != nil {
			return err
		}
		defer resp.Body.Close()

		// Create our progress reporter and pass it to be used alongside our writer
		counter := &WriteCounter{0, 0}
		_, err = io.Copy(out, io.TeeReader(resp.Body, counter))
		if err != nil {
			return err
		}
		out.Close()

		err = os.Rename(filepath+".tmp", filepath)
		if err != nil {
			log.Fatal(err)
			return err
		}

		e := math.Floor(math.Log(float64(counter.Total)) / math.Log(1024))
		fmt.Printf("%.1f%cB", float64(counter.Total)/math.Pow(1024, e), " KMGTP"[int(e)])
	} else {
		fmt.Printf(" is cached")
	}

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
			fmt.Printf(".")
		} else {
			wc.Previous -= scale
			break
		}
	}
	return n, nil
}

func Unzip(src string, dest string) ([]string, error) {

	var filenames []string

	r, err := zip.OpenReader(src)
	if err != nil {
		return filenames, err
	}
	defer r.Close()

	for _, f := range r.File {

		rc, err := f.Open()
		if err != nil {
			return filenames, err
		}
		defer rc.Close()

		// Store filename/path for returning and using later on
		fpath := filepath.Join(dest, f.Name)
		filenames = append(filenames, fpath)

		if f.FileInfo().IsDir() {

			// Make Folder
			os.MkdirAll(fpath, os.ModePerm)

		} else {

			// Make File
			if err = os.MkdirAll(filepath.Dir(fpath), os.ModePerm); err != nil {
				return filenames, err
			}

			outFile, err := os.OpenFile(fpath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, f.Mode())
			if err != nil {
				return filenames, err
			}

			_, err = io.Copy(outFile, rc)

			// Close the file without defer to close before next iteration of loop
			outFile.Close()

			if err != nil {
				return filenames, err
			}

		}
	}
	return filenames, nil
}
