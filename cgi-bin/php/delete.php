<?php

$upload_dir = getenv('UPLOAD_DIR');
$deleted = false;
$error_message = '';

// Only allow POST requests with filename
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['filename'])) {
	$filename = $_POST['filename'];
	
	// Security checks
	$filename = basename($filename); // Prevent directory traversal
	
	if (!empty($filename)) {
		$file_path = $upload_dir . $filename;
		
		// Verify before deletion
		if (file_exists($file_path) && is_writable($file_path)) {
			$deleted = unlink($file_path);
			if (!$deleted) {
				$error_message = 'Delete operation failed (server error)';
			}
		} else {
			$error_message = 'File not found or not writable';
		}
	} else {
		$error_message = 'Invalid filename';
	}
} else {
	$error_message = 'Invalid request method or missing filename';
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
		<?php endif; ?>

		<div style="margin-top: 2rem;">
			<form method="POST" action="upload.php" style="display: inline;">
				<button type="submit" name="back" value="1" class="button">← Back to Upload</button>
			</form>
		</div>
	</div>
</body>
</html>
