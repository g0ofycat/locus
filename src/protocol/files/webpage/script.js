const params = new URLSearchParams(window.location.search);
const id = params.get("id");

const status = document.querySelector("#status");
const fileId = document.querySelector("#file-id");
const downloadLink = document.querySelector("#download-link");

if (id && /^\d+$/.test(id)) {
	fileId.textContent = `File share ID: ${id}`;
	fileId.hidden = false;


	downloadLink.href = `/files?id=${encodeURIComponent(id)}`;
	downloadLink.hidden = false;

	status.textContent = "This link is ready to download.";
} else {
	status.textContent = "This link is missing a valid file ID.";
}

