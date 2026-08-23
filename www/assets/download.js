document.addEventListener("DOMContentLoaded", () => {
  const downloadUserFolder = document.getElementById("download-user-folder");

  downloadUserFolder.addEventListener("click", async () => {
    if (typeof JSZip === "undefined") return;

    downloadUserFolder.disabled = true;
    downloadUserFolder.innerText = "Downloading User Folder...";

    try {
      if (typeof Module !== "undefined" && Module.FS && Module.FS.syncfs) {
        await new Promise((resolve) => {
          Module.FS.syncfs(false, function (err) {
            if (err) console.warn("Emscripten FS: ", err);
            resolve();
          });
        });
      }

      const req = indexedDB.open("/storage");
      req.onsuccess = function (event) {
        const db = event.target.result;
        if (!db.objectStoreNames.contains("FILE_DATA")) {
          alert("No file data found");
          resetDUF();
          return;
        }

        const zip = new JSZip();
        let numFiles = 0;

        const cursorReq = db.transaction("FILE_DATA", "readonly").objectStore("FILE_DATA").openCursor();
        cursorReq.onsuccess = function (e) {
          const cursor = e.target.result;
          if (cursor) {
            const key = cursor.key;
            const record = cursor.value;

            if (record && record.contents) {
              let data = record.contents.contents !== undefined ? record.contents.contents : record.contents;
              if (!(record.mode && record.mode & 0x4000)) {
                const payload =
                  data instanceof Uint8Array || data instanceof ArrayBuffer ? data : data.buffer ? data.buffer : null;
                if (payload) {
                  let path = String(key || `file_${numFiles}`);
                  path = path.replace(/^\/+/, "");
                  const prefix = "storage/toggins/Klawiatura/";
                  if (path.startsWith(prefix)) path = path.substring(prefix.length);

                  zip.file(path, payload);
                  ++numFiles;
                }
              }
            }

            cursor.continue();
          } else {
            if (numFiles === 0) {
              alert("User folder is empty");
              resetDUF();
              return;
            }

            zip
              .generateAsync({ type: "blob" })
              .then(function (content) {
                const blob = URL.createObjectURL(content);
                const download = document.createElement("a");
                download.href = blob;
                download.download = "Klawiatura-user.zip";
                document.body.appendChild(download);
                download.click();
                document.body.removeChild(download);
                URL.revokeObjectURL(blob);

                resetDUF();
              })
              .catch((err) => {
                alert("Failed to generate ZIP: " + err.message);
                resetDUF();
              });
          }
        };

        cursorReq.onerror = () => {
          alert("Failed to read database records");
          resetDUF();
        };
      };
      req.onerror = () => {
        alert("Failed to open local storage");
        resetDUF();
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
