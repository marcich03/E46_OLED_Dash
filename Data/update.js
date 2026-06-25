document.getElementById('uploadButton').addEventListener('click', function() {
    const fileInput = document.getElementById('fileInput');
    const file = fileInput.files[0];
    const status = document.getElementById('status');
    const progress = document.getElementById('progress');
    const progressBar = document.getElementById('progressBar');

    if (!file) {
        status.textContent = 'Proszę wybrać plik!';
        status.style.color = 'red';
        return;
    }

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);

    xhr.upload.onprogress = function(event) {
        if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            progressBar.style.width = percentComplete.toFixed(2) + '%';
            progressBar.textContent = percentComplete.toFixed(2) + '%';
        }
    };

    xhr.onloadstart = function() {
        status.textContent = 'Rozpoczynanie wysyłania...';
        status.style.color = 'white';
        progress.style.display = 'block';
        progressBar.style.width = '0%';
        progressBar.textContent = '0%';
    };

    xhr.onload = function() {
        if (xhr.status === 200) {
            status.textContent = 'Aktualizacja zakończona sukcesem! Urządzenie zostanie zrestartowane.';
            status.style.color = 'green';
        } else {
            status.textContent = 'Błąd podczas aktualizacji: ' + xhr.responseText;
            status.style.color = 'red';
        }
    };

    xhr.onerror = function() {
        status.textContent = 'Błąd sieciowy podczas wysyłania pliku.';
        status.style.color = 'red';
    };

    const formData = new FormData();
    formData.append('update', file);
    xhr.send(formData);
});