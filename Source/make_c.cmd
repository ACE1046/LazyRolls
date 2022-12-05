@echo off
cd data
7z a -tgzip -mx9 scripts.js.gz scripts.js
7z a -tgzip -mx9 styles.css.gz styles.css

php -n make_c.php > spiff_files.h
echo Move generated "spiff_files.h" to source dir
copy spiff_files.h ..\LazyRolls\
cd ..
