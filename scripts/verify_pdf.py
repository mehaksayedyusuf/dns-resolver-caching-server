import fitz

doc = fitz.open('DOCUMENTATION/Team11_Chapter4_Sprint1_Handbook_Final.pdf')
print('PDF Page Count:', len(doc))
for i in range(len(doc)):
    page = doc[i]
    text = page.get_text()
    lines = [l.strip() for l in text.split('\n') if l.strip()]
    first_few = ' // '.join(lines[:3]) if lines else 'EMPTY'
    images = len(page.get_images())
    print(f"Page {i+1}: text_len={len(text)}, images={images}, snippet={first_few[:70]}")
