# Dependencies

```apt-get install libjansson-dev libssl-dev zlib1g-dev```

The dependecies are minimal as the build compiles mupdf from a source tar and mupdf compiles most of it's own dependencies.

# Build instructions

The usual cmake process

```mkdir build-dir
cd build-dir
cmake path/to/src
make
```

fillpdf should now be ready for use in build-dir


# Usage

fillpdf needs three input files. 

1. A json template which maps pdf form field names to the input data name. The root json node is an object, property names on the root object are page numbers, the values for each page is an array containing all form fields on that page (extra textfields and digital signature can also be added here)
2. An input data json file. Root node is an object, it's keys are input data names, the values are inserted into the pdf form.
3. The pdf file

The skeleton template should be generated with `fillpdf -j new_tpl.json fill_this.pdf` or dumped to stdout usinf `fillpdf -t fill_this.pdf`

eg
```
new_tpl.json
{
  "0": [
    {
      "id": 741,
      "name": "f1_1[0]",
      "key": ""
    },
    {
      "id": 742,
      "name": "f1_2[0]",
      "key": ""
    },
    ...
}
```

Now the template file must be completed by adding the "key" value for each form field you want fill (and adding extra fields required).

To help fill in the template generate a pdf file with the object numbers annotations using`fillpdf -a annotated.pdf fill_this.pdf`. It makes annotated.pdf a clone of fill_this.pdf so you know which field on the pdf relates to which item in the template file and meaningful data names can be used.


```
new_tpl.json
{
  "0": [
    {
      "id": 741,
      "name": "f1_1[0]",
      "key": "personal_name"
    },
    {
      "id": 742,
      "name": "f1_2[0]",
      "key": "business_name"
    },
    ...
}
```

Now the fillpdf tool can complete the form. With input data like

```
input_data.json
{
	 "personal_name": "A. Smith", 
	 "business_name": "SmithCo",
   ...
}
```

# Then

```
fillpdf -m new_tpl.json -d input_data.json fill_in.pdf output.pdf
``` 

Will fill in all fields listed in input_data (ignoring any data which is not set) and write it to output.pdf.

# Add things
If you want to sign the document add a new signature item to the items of any page in new_tpl.json


```
{
      "add": "signature",
      "key": "addsig"
}
```
`
It also needs `"addsig": true` in the input_data.json. 

To make the signature visible it the add: signature item needs to specify the font and size. Fonts are only partly support by the fillpdf tool at the moment and only works with fonts that are already defined in the input.pdf. Use `fillpdf -f input.pdf` and choose one of the few fonts listed in widget_fonts property. eg

```
{
      "add": "signature",
      "font": "Helv",
      "rect": {"left": 490, "top":510, "width": 40, "height": 40},
      "key": "addsig"
}
```

Textfields can be added in a similar method to signature by changing "add":"signature" to "add":"textfield"

# To do
Most useful features would be better (proper) support for fonts, being able to add images, adding text without pretending it's non-editable a textfield, annotations too. Possibly making the clunky template prep optional and adding everything to a more complex input_data json. 
