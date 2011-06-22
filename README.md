#rdf-raptor-parser

A Node.js extension for the Raptor RDF parser library.

## Installation

    $npm install rdf-raptor-parser

The extension has been tested in GNU/Linux Debian 6.0 and OSX 10.5

## Dependencies

- libraptor v 1.4 must be installed in the system.

## Examples

Parsing a file:

    var Parser = require("rdf-raptor-parser");
     
    var parser = new Parser.RaptorParser("text/turtle");
     
    var counter = 0;
     
    parser.addListener("parsed_triple",function(triple){
      counter++
    });
     

    parser.addListener("error_parsing",function(){
        console.log("Something went terribly wrong");
    });     

    parser.addListener("finished_parsing", function(){
      console.log("\nparsed " + counter + " triples parsed");
    });
     
    parser.parseFile("./test/data/sp2b_10k.n3");

Parsing a string:

    var parser = new Parser.RaptorParser("application/rdf+xml");

    var toParse = '<?xml version="1.0"?>\
                   <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\
                            xmlns:contact="http://www.w3.org/2000/10/swap/pim/contact#">\
                     <contact:Person rdf:about="http://www.w3.org/People/EM/contact#me">\
                       <contact:fullName>Eric Miller</contact:fullName>\
                       <contact:mailbox rdf:resource="mailto:em@w3.org"/>\
                       <contact:personalTitle>Dr.</contact:personalTitle> \
                     </contact:Person>\
                   </rdf:RDF>';

    parser.addListener("parsed_triple",function(triple){
        console.log(t);
    });     

    parser.parse(toParse);

Parsing a URI:

    var parser = new Parser.RaptorParser("text/turtle");

    // set up handlers

    parser.parseUri("http://dbpedialite.org/things/18016.ttl");


## Author

  Antonio Garrote, antoniogarrote@gmail.com

## License

  For Raptor licensing information, see: http://librdf.org/raptor/LICENSE.html
   
    

