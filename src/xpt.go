package main

import "os"
import "fmt"
import "runtime"

func main() {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")
	args := os.Args[1:]
	if len(args) < 2 {usage();os.Exit(1)}
	if args[0] != "install" {usage();os.Exit(1)}
	args = args[1:] // Оставляем только package1 @ tag1 package2 package3 @ tag3 package4
	for _, element := range args {
		fmt.Println(element)
	}
}

type xptPackage struct {
	name string
	tag string
	url string
	size string
}

func usage() {
	fmt.Println("use: xpt install package1 @ tag1 package2 package3 @ tag3 package4")
}