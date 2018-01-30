package main

import "os"
import "fmt"
import "runtime"

func main() {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")

	if len(os.Args) == 2 && os.Args[1] == "update" {
		os.Exit(update())
	} else if len(os.Args) > 2 && os.Args[1] == "install" {
		os.Exit(install())
	} else {
		usage()
		os.Exit(1)
	}

	// for _, element := range args {
	// 	fmt.Println(element)
	// }
}

func update() int {
	fmt.Println("Update")
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

func usage() {
	fmt.Println("use: xpt install package1 @ tag1 package2 package3 @ tag3 package4")
}
