for f in components/screen_*/ui_Screen*.h; do
  name=$(basename "$f" .h | sed 's/^ui_//')
  grep -q "${name}_full_init" "$f" || \
    sed -i "s/extern void ui_${name}_screen_destroy(void);/extern void ui_${name}_screen_destroy(void);\nextern void ui_${name}_full_init(void);/" "$f"
done