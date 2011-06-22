var Parser = require("../build/default/raptor_parser");

exports.testLoadURI = function(test) {
    var parser = new Parser.RaptorParser("text/turtle");

    var counter = 0;

    parser.addListener("parsed_triple",function(triple){
        counter++;
    });
     
    parser.addListener("finished_parsing", function(){
        test.ok(counter>0);
        test.done();
    });
     
    parser.parseUri("http://dbpedialite.org/things/18016.ttl");
}
