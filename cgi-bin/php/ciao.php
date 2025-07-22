#!/usr/bin/php-cgi
<?php

// Get the name parameter from GET or POST
$name = 'Guest';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $name = isset($_POST['name']) ? htmlspecialchars($_POST['name']) : $name;
} else {
    $name = isset($_GET['name']) ? htmlspecialchars($_GET['name']) : $name;
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Greeting</title>
    <link rel="stylesheet" href="/www/styles.css" />
	<link rel="icon" type="image/x-icon" href="/www/favicon.png">
</head>
<body>
    <div class="floating-elements">
        <div class="floating-element"></div>
        <div class="floating-element"></div>
        <div class="floating-element"></div>
    </div>

    <div class="container">
        <h1 class="title">Hello, <?php echo $name; ?>!</h1>
        <p class="subtitle">Nice to meet you.</p>

        <a href="/www/index.html" class="button">Back to Home</a>
    </div>
</body>
</html>