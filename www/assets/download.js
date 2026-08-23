document.addEventListener("DOMContentLoaded", () => {
  const downloadUserFolder = document.getElementById("download-user-folder");
  downloadUserFolder.addEventListener("click", async () => {
    if (typeof JSZip === "undefined") {
      alert("JSZip not found");
      return;
    }

    downloadUserFolder.disabled = true;
    downloadUserFolder.innerText = "Downloading User Folder...";

    try {
      if (typeof Module !== "undefined" && Module.FS && Module.FS.syncfs) {
        await new Promise((resolve, reject) => {
          Module.FS.syncfs(false, function (err) {
            if (err) console.warn("Emscripten FS: ", err);
            resolve();
          });
        });
      }

      let dbName = null;
      try {
        const databases = await indexedDB.databases();
        const targetDB = databases.find((db) => db.name.includes("emscripten") || db.name.startsWith("/"));
        if (targetDB) dbName = targetDB.name;
      } catch (e) {
        console.warn("Indexed DB databases not available, falling back to default path");
      }
      if (!dbName) dbName = "/emscripten/fs";

      const request = indexedDB.open(dbName);
      request.onsuccess = function (event) {
        const db = event.target.result;
        if (!db.objectStoreNames.contains("FILE_DATA")) {
          alert("No file data found");
          resetDUF();
          return;
        }

        const transaction = db.transaction("FILE_DATA", "readonly");
        const objectStore = transaction.objectStore("FILE_DATA");
        const getAllRequest = objectStore.getAll();
        getAllRequest.onsuccess = function () {
          const records = getAllRequest.result;
          if (!records || records.length === 0) {
            alert("User folder is empty");
            resetDUF();
            return;
          }

          const zip = new JSZip();
          let numFiles = 0;

          records.forEach((record, index) => {
            if (record && record.contents) {
              if (
                !(
                  record.contents instanceof Uint8Array ||
                  record.contents instanceof ArrayBuffer ||
                  record.contents instanceof Blob ||
                  typeof record.contents === "string"
                )
              )
                return;

              let byteLength = 0;
              if (record.contents.byteLength !== undefined) {
                byteLength = record.contents.byteLength;
              } else if (record.contents.length !== undefined) {
                byteLength = record.contents.length;
              } else if (typeof record.contents === "object") {
                byteLength = Object.keys(record.contents).length;
              }
              if (byteLength === 0) return;

              let path = record.id || `file_${index}`;
              path = path.replace(/^\/+/, "");
              zip.file(path, record.contents);

              ++numFiles;
            }
          });

          if (numFiles === 0) {
            alert("User folder is empty");
            resetDUF();
            return;
          }

          zip
            .generateAsync({ type: "blob" })
            .then(function (content) {
              const blobURL = URL.createObjectURL(content);
              const downloadLink = document.createElement("a");
              downloadLink.href = blobURL;
              downloadLink.download = "Klawiatura-user.zip";
              document.body.appendChild(downloadLink);
              downloadLink.click();
              document.body.removeChild(downloadLink);
              URL.revokeObjectURL(blobURL);

              resetDUF();
            })
            .catch((err) => {
              alert("Failed to generate ZIP: " + err.message);
              resetDUF();
              return;
            });
        };
      };
      request.onerror = () => {
        alert("Failed to open local storage");
        resetDUF();
        return;
      };
    } catch (error) {
      console.error(error);
      alert(error.message);
      resetDUF();
    }

    function resetDUF() {
      downloadUserFolder.disabled = false;
      downloadUserFolder.innerText = "Download User Folder";
    }
  });
});
