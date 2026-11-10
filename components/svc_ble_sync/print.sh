find . -type f -print0 | while IFS= read -r -d '' file; do
    echo "===== $file ====="
    if [[ $(basename "$file") == "lv_font_montserrat_14_cyrillic.c" ]]; then
        echo "---"
    else
        cat "$file"
    fi
    echo
done > all_files.txt