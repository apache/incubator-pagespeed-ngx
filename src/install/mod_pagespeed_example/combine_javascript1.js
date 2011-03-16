// Appends a plaintext message to the body, followed by a new line.
function logText(message) {
  document.body.appendChild(document.createTextNode(message));
  document.body.appendChild(document.createElement("br"));
}
