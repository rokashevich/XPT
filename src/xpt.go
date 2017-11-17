package main

import "os"
import "fmt"
import "runtime"

func main() {
	fmt.Println("xpt ver. 0.0.0 (" + runtime.Version() + ")")
	args := os.Args[1:]
	fmt.Println(args)
}