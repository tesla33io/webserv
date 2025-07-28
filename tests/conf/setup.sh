#!/bin/bash

for i in $(seq 3); do
    mkdir /tmp/server$i;
    html_content='
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>'"$i"' Test Page</title>
  <style>
    body {
      font-family: Comic Sans MS, cursive, sans-serif;
      background-color: #fef9e7;
      color: #333;
      text-align: center;
      margin-top: 10%;
    }
    h1 {
      font-size: 3em;
      color: #e74c3c;
      animation: wobble 1s infinite;
    }
    p {
      font-size: 1.2em;
      color: #2c3e50;
    }
    button {
      background-color: #3498db;
      color: white;
      border: none;
      padding: 10px 20px;
      font-size: 1em;
      border-radius: 5px;
      cursor: pointer;
    }
    button:hover {
      background-color: #2980b9;
    }
    @keyframes wobble {
      0%, 100% { transform: rotate(0deg); }
      25% { transform: rotate(3deg); }
      75% { transform: rotate(-3deg); }
    }
  </style>
</head>
<body>
  <h1>💥 Hello, <span id="name">WORLD'"$i$i$i"'</span>! 💥</h1>
  <p>This is a silly test page. You are totally doing science here.</p>
  <button onclick="changeName()">Click me for magic</button>

  <script>
    function changeName() {
      const names = ["WORLD'"$i$i$i"'", "HUMAN'"$i$i$i"'", "YOU'"$i$i$i"'", "'"$i$i$i"'NERD", "'"$i$i$i"'BASHER"];
      const current = document.getElementById("name").innerText;
      let next = names[Math.floor(Math.random() * names.length)];
      while (next === current) {
        next = names[Math.floor(Math.random() * names.length)];
      }
      document.getElementById("name").innerText = next;
    }
  </script>
</body>
</html>'
    echo $html_content > /tmp/server$i/index.html;
done
