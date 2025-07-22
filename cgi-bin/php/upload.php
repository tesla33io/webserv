#!/usr/bin/php-cgi

<?php
$upload_dir = "./uploads/";

// Create upload directory if it doesn't exist
if (!is_dir($upload_dir)) {
    mkdir($upload_dir, 0755, true);
}

// Get uploaded file
$uploaded_file = $_FILES['file'];

// Sanitize filename
$filename = basename($uploaded_file['name']);
$filename = preg_replace('/[^a-zA-Z0-9._-]/', '', $filename);

if (empty($filename)) {
    $filename = 'uploaded_file_' . time();
}

// Handle duplicate filenames
$target_path = $upload_dir . $filename;
if (file_exists($target_path)) {
    $filename = time() . '_' . $filename;
    $target_path = $upload_dir . $filename;
}

// Move uploaded file
move_uploaded_file($uploaded_file['tmp_name'], $target_path);

// Return success response
echo json_encode(array(
    'filename' => $filename,
    'size' => $uploaded_file['size']
));
?>