<?php

// Prevent script from printing default headers
ini_set('default_mimetype', '');

// Start output buffering to calculate content length
ob_start();

// Default name if none provided
$name = 'Guest';
$exit_code = '200';

// Check if 'name' is set in the URL query parameters
if (isset($_GET['name'])) {

	// Remove leading/trailing whitespace
	$name = trim($_GET['name']);

	// Limit the name to 50 characters to avoid abuse or layout issues
	if (strlen($name) > 50) {
		$name = substr($name, 0, 50);
	}

	// Allow only letters, numbers, spaces, periods, apostrophes, and hyphens
	// This prevents script injections and unwanted symbols
	if (!preg_match('/^[\p{L}\p{N} .\'\-]+$/u', $name)) {
		$name = 'Guest'; // Reset to default if input is invalid
	}

	// Convert special HTML characters to safe entities (e.g., < becomes &lt;)
	// Prevents Cross-Site Scripting (XSS)
	$name = htmlspecialchars($name);
}

?>

<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>Greeting</title>
	<link rel="stylesheet" href="/styles.css" />
	<link rel="icon" type="image/x-icon" href="/favicon.png">
</head>
<body>
	<div class="floating-elements">
		<div class="floating-element"></div>
		<div class="floating-element"></div>
		<div class="floating-element"></div>
	</div>

	<div class="container">
		<h1 class="title">Hello, <?php echo $name; ?>!</h1>
		<p class="subtitle">Nice to meet you and welcome to WhateverX :)</p>
		
		<div class="form-row" style="justify-content: center; margin-top: 40px;">
			<a href="/index.html" class="button">Back to Home</a>
		</div>
	</div>
</body>
</html>

<?php

// Calculate length
$content = ob_get_contents();
ob_end_clean();
$content_length = strlen($content);

echo $exit_code . "\n";

// Output content
echo $content;

?>