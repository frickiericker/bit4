package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"time"
)

type config struct {
	DatabaseURL string `json:"database_url"`
	Token       string `json:"token"`
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func run() error {
	for _ = range time.Tick(10*time.Second) {
		c, err := loadConfig()
		if err != nil {
			fmt.Println("Error:", err)
		} else {
			fmt.Println("DatabaseURL:", c.DatabaseURL)
			fmt.Println("Token:", c.Token)
		}
	}
	return nil
}

func loadConfig() (*config, error) {
	file, err := os.Open("/srv/secrets/config.json")
	if err != nil {
		return nil, err
	}
	defer file.Close()

	content, err := ioutil.ReadAll(file)
	if err != nil {
		return nil, err
	}

	var c config
	if err := json.Unmarshal(content, &c); err != nil {
		return nil, err
	}
	return &c, nil
}
