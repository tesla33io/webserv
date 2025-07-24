#!/usr/bin/php-cgi

<?php
$upload_dir = "./uploads/";
$deleted = false;
$error_message = '';

// Only allow POST requests with filename
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['filename'])) {
    $filename = $_POST['filename'];
    
    // Security checks
    $filename = basename($filename); // Prevent directory traversal
    $filename = preg_replace('/[^a-zA-Z0-9._-]/', '', $filename); // Match upload.php's sanitization
    
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
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title><?= $deleted ? 'Deleted' : 'Error' ?></title>
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
            <a href="upload.php" class="button">← Back to Upload</a>
        </div>
    </div>
</body>
</html>