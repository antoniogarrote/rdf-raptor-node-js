var Parser = require("../build/default/raptor_parser");

exports.testLoadXML = function(test) {

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

    var counter = 0;
    
    parser.addListener("parsed_triple",function(triple){
        counter++
    });

    parser.addListener("finished_parsing", function(){
        var after = new Date().getTime();
        test.ok(counter==4);
        test.done();
    });
    
    var before = new Date().getTime();
    parser.parse(toParse);
}
