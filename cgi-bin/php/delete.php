<?php
// Prevent script from printing default headers
ini_set('default_mimetype', '');

// Start output buffering to calculate content length
ob_start();

$upload_dir = getenv('UPLOAD_DIR');
$deleted = false;
$error_message = '';
$exit_code = '200';
$filename = '';

// Handle both DELETE and POST methods
if ($_SERVER['REQUEST_METHOD'] === 'DELETE') {
	// For true DELETE requests, filename comes from query string or request body
	if (isset($_GET['filename'])) {
		$filename = $_GET['filename'];
	} else {
		// Try to read from request body
		$input = file_get_contents('php://input');
		if ($input) {
			// Parse as query string if it looks like one
			if (strpos($input, '=') !== false) {
				parse_str($input, $data);
				if (isset($data['filename'])) {
					$filename = $data['filename'];
				}
			} else {
				// Treat entire body as filename
				$filename = trim($input);
			}
		}
	}
} elseif ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['_method']) && $_POST['_method'] === 'DELETE') {
	// Fallback for form-based deletion
	$filename = $_POST['filename'] ?? '';
} else {
	$error_message = 'Invalid request method';
	$exit_code = '405';
}

// Process deletion if we have a filename
if (empty($error_message) && !empty($filename)) {
	// Security checks
	$filename = basename($filename); // Prevent directory traversal
	
	if (!empty($filename)) {
		$file_path = $upload_dir . $filename;
		
		// Verify before deletion
		if (file_exists($file_path) && is_writable($file_path)) {
			$deleted = unlink($file_path);
			if (!$deleted) {
				$error_message = 'Delete operation failed (server error)';
				$exit_code = '502';
			}
		} else {
			if (!file_exists($file_path)) {
				$error_message = 'File not found';
				$exit_code = '404';
			} else if (!is_writable($file_path)) {
				$error_message = 'File not writable';
				$exit_code = '403';
			}
		}
	} else {
		$error_message = 'Invalid filename';
		$exit_code = '400';
	}
} elseif (empty($error_message)) {
	$error_message = 'No filename provided';
	$exit_code = '400';
}

if ($error_message != '') {
	$stderr = fopen('../../script.log', 'a');
	fwrite($stderr, (new DateTime())->format(DateTime::ATOM) . " " . $error_message . "\n" . PHP_EOL);
	fclose($stderr);
}

?>

<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title><?= $deleted ? 'Deleted' : 'Error' ?></title>
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
		<?php if ($deleted): ?>
			<div style="font-size: 3rem; margin-bottom: 1rem;">🗑️</div>
			<h1 class="title">File deleted</h1>
			<p class="subtitle"><?= htmlspecialchars($filename) ?> was removed</p>
		<?php else: ?>
			<div style="font-size: 3rem; margin-bottom: 1rem;">❌</div>
			<h1 class="title">Deletion Failed</h1>
			<p class="subtitle"><?= htmlspecialchars($error_message) ?></p>
			<p style="font-size: 0.8em; opacity: 0.7;">Method: <?= htmlspecialchars($_SERVER['REQUEST_METHOD']) ?></p>
		<?php endif; ?>

		<div style="margin-top: 2rem;">
			<form method="POST" action="upload.php" style="display: inline;">
				<button type="submit" name="back" value="1" class="button">← Back to Upload</button>
			</form>
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
