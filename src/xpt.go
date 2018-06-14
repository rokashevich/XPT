package main

import (
	"fmt"
	"io/ioutil"
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

	//
	// Настраиваем cache-директорию.
	//
	xptCache := os.Getenv("XPTCACHE")
	if xptCache == "" { // Если XPTCACHE не задан, то используем домашний каталог.
		u, _ := user.Current()
		xptCache = u.HomeDir
	}
	// Превращаем D:\svn\program_src в D_svn_program_src для уникальности xptCache
	reg, _ := regexp.Compile("[^a-zA-Z0-9]+")
	xptCache = filepath.Join(xptCache, "xptcache", reg.ReplaceAllString(sandbox, "_"))
	if _, err := os.Stat(xptCache); os.IsNotExist(err) {
		os.MkdirAll(xptCache, os.ModePerm)
	}

	//
	// Обрабатываем аргументы командной строки.
	//
	if len(os.Args) == 2 && os.Args[1] == "update" {
		os.Exit(update())
	} else if len(os.Args) > 2 && os.Args[1] == "install" {
		os.Exit(install())
	} else {
		os.Exit(usage())
	}
}

// var/xpt/update/
//               /packages_without_tag/
//               /tag1/
//                    /package1.txt
//                    /package2.txt
//               /tag2/
func update() int {
	dat, err := ioutil.ReadFile("sources.txt")
	if err != nil {
		os.Exit(1)
	}
	for _, line := range strings.Split(string(dat), "\n") {
		if strings.HasPrefix(line, "repo ") {
			fmt.Println(line)
			for _, word := range strings.Split(line, " ") {
				fmt.Println("!" + word + "?")
			}
		}
	}
	return 0
}

func install() int {
	fmt.Println("Install")
	return 0
}

type xptPackage struct {
	name string
	tag  string
	url  string
	size string
}

func usage() int {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")
	fmt.Println("usage: xpt install package1 @ tag1 package2 package3 @ tag3 package4")
	return 1
}
