7z a -tgzip scripts.js.gz scripts.js
7z a -tgzip styles.css.gz styles.css

php -n make_c.php > spiff_files.h
echo Move generated "spiff_files.h" to source dir
copy spiff_files.h ..\LazyRolls\
pause