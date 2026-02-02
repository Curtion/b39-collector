package main

import (
	"embed"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"log"
	"net/http"
)

//go:embed all:web/dist
var webDist embed.FS

func main() {
	distFS, err := fs.Sub(webDist, "web/dist")
	if err != nil {
		log.Fatal(err)
	}

	http.HandleFunc("/api/data", handleData)
	http.Handle("/", http.FileServer(http.FS(distFS)))

	log.Println("Starting server on :8080")
	log.Fatal(http.ListenAndServe(":8080", nil))
}

func handleData(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "Failed to read body", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	var data map[string]interface{}
	if err := json.Unmarshal(body, &data); err != nil {
		fmt.Printf("Raw body: %s\n", string(body))
	} else {
		prettyJSON, _ := json.MarshalIndent(data, "", "  ")
		fmt.Printf("Received JSON:\n%s\n", string(prettyJSON))
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"status": "success"})
}
