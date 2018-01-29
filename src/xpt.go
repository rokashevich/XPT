package main

import "os"
import "fmt"
import "runtime"

func main() {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")

	command := os.Args[1]
	if command != "install" {
		usage()
		os.Exit(1)
	}

	args := os.Args[2:] // package1 @ tag1 package2 package3 @ tag3 package4
	if len(args) < 1 {
		usage()
		os.Exit(1)
	}

	for _, element := range args {
		fmt.Println(element)
	}
}

type xptPackage struct {
	name string
	tag  string
	url  string
	size string
}

func usage() {
	fmt.Println("use: xpt install package1 @ tag1 package2 package3 @ tag3 package4")
}
