package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"runtime"
	"strings"
)

func main() {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")

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
	for _, element := range strings.Split(string(dat), "\n") {
		if strings.HasPrefix(element, "repo ") {
			fmt.Println(element)
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
	fmt.Println("use: xpt install package1 @ tag1 package2 package3 @ tag3 package4")
	return 1
}
