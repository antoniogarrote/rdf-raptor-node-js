
var Parser = require("../build/default/raptor_parser");

exports.testParseError = function(test) {
    var parser = new Parser.RaptorParser("text/turtle");

    var toParse = '@prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\
                   @prefix dc: <http://purl.org/dc/elements/1.1/> .\
                   @prefix ex: <http://example.org/stuff/1.0/> .\
                   <http://www.w3.org/TR/rdf-syntax-grammar>\
                     dc:title "RDF/XML Syntax Specification (Revised)" ;\
                     ERROR .';



    parser.addListener("error_parsing",function(){
        test.ok(true);
        test.done();
    });

    parser.parse(toParse);
}
