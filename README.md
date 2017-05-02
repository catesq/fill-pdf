# Install dependencies

```apt-get install libjansson-dev libssl-dev zlib1g-dev```

The dependencies are minimal as mupdf is built from source and mudpf source packages contain patched versions of it's dependencies.

# Build

The usual cmake process:

```mkdir build-dir
cd build-dir
cmake path/to/src
make
```

fillpdf should now be ready for use in build-dir

# Basic usage

```fillpdf <command> [options] input.pdf [output_file]```

Where command is one of: `annot, info, template, fonts, complete`

There is a mandatory setup step: 
```
fillpdf template input.pdf template.json
```
Will build a template containing the name, object id and input widget type of all the data fields on each page of the pdf. 

Then:
```
fillpdf complete -d input_data.json -t template.json input.pdf complete.pdf
```
Where input_data.json is a json file with a single object where the keys are the field names and values are data to insert into the pdf. 

# Walk through

Using the fw9.pdf form in examples directory as a guide.

The skeleton template generated with `fillpdf template fw9.pdf fw9_template.json looks something like this`

"0" is a page number, the value are a json list of fields
```
fw9_template.json
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
The name's are often machine generated nonsense and it's usually unclear which name in the template releates to which field on the pdf so it is recommmended to fill in the "key" property with meaningful data names 

So the input data can be written with meaningful names like:

```
input_data.json
{
	 "personal_name": "A. Smith", 
	 "business_name": "Smith Business",
   ...
}
```

To do this use the annot command:
```
fillpdf annot fw9.pdf fw9_annotated.pdf
```

It create a pdf with the object ID of each field annotated in bright red over the middle of each object - trying to edit the fw9_template is guesswork without it.

Using the annotated pdf it's easy to add sensible names to the template:

```
fw9_template_edited.json
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

# Add signatures to the template

To digitally sign the document you must add a signature item to the template. Append this to the list of fields 


```
{
      "add": "signature",
      "key": "addsig",
      "rect":  {"left": 490, "top":510, "width": 40, "height": 40},
}
```

The "rect" item is mandatory. The value of key can be whatever you like. 
Note that the digital signature is only added when when input_data.json contains `"addsig": true`. When "addsig" is false or undefined it is not. 

There can be an optional font property on the "signature" item. fillpdf currently doesn't have much support for fonts and can only use fonts which are available for use by widgets in that pdf - use one of the fonts the "widget_fonts" list returned `fillpdf fonts fw9.pdf`. 

# Add textfields using template

Textfields can be added in a similar method to signature by changing "add":"signature" to "add":"textfield".

# To do
Most useful features would be better (proper) support for fonts, being able to add images, adding text without pretending it's a non-editable textfield, annotations maybe. Possibly making the clunky template prep optional and adding everything to a more complex input_data json. 
