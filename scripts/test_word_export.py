import win32com.client
import os

word = None
try:
    word = win32com.client.Dispatch('Word.Application')
    word.Visible = False
    doc_path = os.path.abspath("DOCUMENTATION/sprint 1'.docx")
    pdf_path = os.path.abspath("DOCUMENTATION/test.pdf")
    doc = word.Documents.Open(doc_path)
    pages = doc.ComputeStatistics(2)
    print(f"Word opened successfully. Page count: {pages}")
    doc.ExportAsFixedFormat(pdf_path, 17) # wdExportFormatPDF = 17
    print("PDF exported successfully.")
    doc.Close(False)
except Exception as e:
    print(f"Error: {e}")
finally:
    if word:
        try:
            word.Quit()
        except:
            pass
