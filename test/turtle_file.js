var Parser = require("../build/default/raptor_parser");

exports.testLoadFile = function(test) {

    var parser = new Parser.RaptorParser("text/turtle");

    var counter = 0;
    
    parser.addListener("parsed_triple",function(triple){
        counter++
    });

    parser.addListener("finished_parsing", function(){
        var after = new Date().getTime();
        test.ok(counter=10303);
        console.log("\nparsed 10303 triples, ellapsed "+(after-before)+" millisecs, "+Math.round((10303/((after-before)/1000))/1000)+"K Triples/sec");
        test.done();
    });
    
    var before = new Date().getTime();
    parser.parseFile("./test/data/sp2b_10k.n3");
}
