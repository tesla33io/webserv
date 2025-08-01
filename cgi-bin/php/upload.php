#!/usr/bin/php-cgi
<?php

$upload_dir = "../../www/uploads/";

// Create upload directory if it doesn't exist
if (!is_dir($upload_dir)) {
    mkdir($upload_dir, 0755, true);
}

$uploaded = false;
$filename = '';
$filesize = 0;
$file_extension = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['file'])) {
    // Get uploaded file
    $uploaded_file = $_FILES['file'];
    
    // Check if file was uploaded successfully
    if ($uploaded_file['error'] === UPLOAD_ERR_OK) {
        // Sanitize filename
        $filename = basename($uploaded_file['name']);
        $filename = preg_replace('/[^a-zA-Z0-9._-]/', '', $filename);
        
        if (empty($filename)) {
            $filename = 'uploaded_file_' . time();
        }
        
        // Get file extension for icon
        $file_extension = strtolower(pathinfo($filename, PATHINFO_EXTENSION));
        
        // Handle duplicate filenames
        $target_path = $upload_dir . $filename;
        if (file_exists($target_path)) {
            $filename = time() . '_' . $filename;
            $target_path = $upload_dir . $filename;
        }
        
        // Move uploaded file
        if (move_uploaded_file($uploaded_file['tmp_name'], $target_path)) {
            $uploaded = true;
            $filesize = $uploaded_file['size'];
        }
    }
}

// Function to get file icon based on extension
function getFileIcon($extension) {
    $icons = array(
        'pdf' => '📄',
        'doc' => '📝',
        'docx' => '📝',
        'txt' => '📄',
        'jpg' => '🖼️',
        'jpeg' => '🖼️',
        'png' => '🖼️',
        'gif' => '🖼️',
        'svg' => '🖼️',
        'mp4' => '🎬',
        'avi' => '🎬',
        'mov' => '🎬',
        'mp3' => '🎵',
        'wav' => '🎵',
        'zip' => '🗜️',
        'rar' => '🗜️',
        'tar' => '🗜️',
        'gz' => '🗜️',
        'html' => '🌐',
        'css' => '🎨',
        'js' => '⚡',
        'php' => '🐘',
        'py' => '🐍',
        'cpp' => '⚙️',
        'c' => '⚙️',
        'java' => '☕',
        'json' => '📋',
        'xml' => '📋',
        'csv' => '📊',
        'xls' => '📊',
        'xlsx' => '📊'
    );
    
    return isset($icons[$extension]) ? $icons[$extension] : '📁';
}

function formatFileSize($bytes) {
    if ($bytes >= 1073741824) {
        return number_format($bytes / 1073741824, 2) . ' GB';
    } elseif ($bytes >= 1048576) {
        return number_format($bytes / 1048576, 2) . ' MB';
    } elseif ($bytes >= 1024) {
        return number_format($bytes / 1024, 2) . ' KB';
    } else {
        return $bytes . ' bytes';
    }
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>File Upload</title>
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
        <?php if ($uploaded): ?>
            <div class="file-icon" style="font-size: 4rem; margin-bottom: 1rem;">
                <?php echo getFileIcon($file_extension); ?>
            </div>
            <h1 class="title">File "<?php echo htmlspecialchars($filename); ?>" successfully uploaded!</h1>
            <p class="subtitle">
                Size: <?php echo formatFileSize($filesize); ?>
                <?php if ($file_extension): ?>
                    | Type: <?php echo strtoupper($file_extension); ?>
                <?php endif; ?>
            </p>
        <?php else: ?>
            <h1 class="title">Upload a File</h1>
            <p class="subtitle">Choose a file to upload to the server.</p>
            
            <form method="POST" enctype="multipart/form-data" style="margin: 2rem 0;">
				<div style="display: flex; gap: 1rem; align-items: center;">
					<input type="file" name="file" required style="flex-grow: 1; padding: 12px 18px; border: 2px solid #e2e8f0; border-radius: 14px; background: white; font-size: 0.95rem; font-family: inherit;">
					<button type="submit" class="button" style="margin-top: 0; height: fit-content;">Upload File</button>
				</div>
			</form>
        <?php endif; ?>

        <?php
        // Display existing files in uploads folder
        $existing_files = array();
        if (is_dir($upload_dir)) {
            $files = scandir($upload_dir);
            foreach ($files as $file) {
                if ($file != '.' && $file != '..' && is_file($upload_dir . $file)) {
                    $existing_files[] = $file;
                }
            }
        }
        
        if (!empty($existing_files)): ?>
            <div style="margin-top: 3rem; padding-top: 2rem; border-top: 1px solid rgba(255,255,255,0.2);">
                <h2 style="color: #64748b; margin-bottom: 1.5rem; text-align: center;">Files in Upload Directory</h2>
                <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 1rem; margin-bottom: 2rem;">
                    <?php foreach ($existing_files as $file): 
                        $file_ext = strtolower(pathinfo($file, PATHINFO_EXTENSION));
                        $file_path = $upload_dir . $file;
                        $file_size = file_exists($file_path) ? filesize($file_path) : 0;
                        $display_name = strlen($file) > 15 ? substr($file, 0, 12) . '...' : $file;
                    ?>
                        <div style="position: relative; text-align: center; padding: 1rem; background: rgba(255,255,255,0.1); border-radius: 12px; backdrop-filter: blur(10px);">
                            <form method="POST" action="delete.php" style="position: absolute; top: 8px; right: 8px; margin: 0;">
                                <input type="hidden" name="filename" value="<?php echo htmlspecialchars($file); ?>">
                                <button type="submit" 
                                        style="background: rgba(255,0,0,0.7); color: white; border: none; border-radius: 50%; width: 20px; height: 20px; font-size: 12px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-weight: bold;">
                                    ×
                                </button>
                            </form>
                            
                            <div style="font-size: 1.8rem; margin-bottom: 0.5rem;">
                                <?php echo getFileIcon($file_ext); ?>
                            </div>
                            <div style="color: white; font-size: 0.75rem; margin-bottom: 0.3rem;" title="<?php echo htmlspecialchars($file); ?>">
                                <?php echo htmlspecialchars($display_name); ?>
                            </div>
                            <div style="color: rgba(255,255,255,0.7); font-size: 0.65rem;">
                                <?php echo formatFileSize($file_size); ?>
                            </div>
                        </div>
                    <?php endforeach; ?>
                </div>
            </div>
        <?php endif; ?>

        <a href="/www/index.html" class="button">Back to Home</a>
    </div>
</body>
</html>