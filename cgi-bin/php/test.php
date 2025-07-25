#!/usr/bin/php-cgi
<?php
// Debug info: dump the superglobals safely
var_dump($_SERVER['QUERY_STRING']);
var_dump($_GET);
var_dump($_POST);

// Correct way to get 'name' from GET or POST
$get_name = isset($_GET['name']) ? $_GET['name'] : 'not set';
$post_name = isset($_POST['name']) ? $_POST['name'] : 'not set';

echo "GET name: " . htmlspecialchars($get_name) . "\n";
echo "POST name: " . htmlspecialchars($post_name) . "\n";
?>
