#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include <string>
#include <raptor.h>

static int counter =0;

using namespace v8;
using namespace node;

struct node_struct {
  char* kind;
  unsigned char* value;
  unsigned char* type;
  const unsigned char* lang;
};

struct triple_struct {
  node_struct* subject;
  node_struct* predicate;
  node_struct* object;
};

static Persistent<String> parsed_triple_symbol;
static Persistent<String> finished_parsing_symbol;
static Persistent<String> error_parsing_symbol;

static  pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_t parser_thread;

static void* ParserThread(void * data);

class RaptorParser : public EventEmitter {
public:

  static Persistent<FunctionTemplate> persistent_function_template;
  ev_async* triple_parsed_async_;
  ev_async* finished_parsed_async_;
  ev_async* error_parsing_async_;
  Handle<String> media_type;
  Handle<String> base_uri;
  Handle<String> to_parse;
  std::queue<triple_struct*>* statements_queue;
  bool foundError;
  std::string kindOfParsing;


  struct triple_out_data {
    RaptorParser* parser_instance;
  };

  RaptorParser() {
    triple_parsed_async_   = new ev_async();
    finished_parsed_async_ = new ev_async();
    error_parsing_async_ = new ev_async();
    statements_queue = new std::queue<triple_struct*>();
    foundError = false;
  };

  ~RaptorParser() {
    delete triple_parsed_async_;
    delete finished_parsed_async_;
    delete error_parsing_async_;
    delete statements_queue;
  };

  static void Init(Handle<Object> target){
    HandleScope scope;

    parsed_triple_symbol = NODE_PSYMBOL("parsed_triple");
    finished_parsing_symbol = NODE_PSYMBOL("finished_parsing");
    error_parsing_symbol = NODE_PSYMBOL("error_parsing");

    Local<FunctionTemplate> local_function_template = FunctionTemplate::New(New);
    persistent_function_template = Persistent<FunctionTemplate>::New(local_function_template);

    persistent_function_template->Inherit(EventEmitter::constructor_template);
    persistent_function_template->InstanceTemplate()->SetInternalFieldCount(1); // 1 since this is a constructor function
    persistent_function_template->SetClassName(String::NewSymbol("RaptorParser"));
    
    NODE_SET_PROTOTYPE_METHOD(persistent_function_template, "parse", Parse);
    NODE_SET_PROTOTYPE_METHOD(persistent_function_template, "parseFile", ParseFile);
    NODE_SET_PROTOTYPE_METHOD(persistent_function_template, "parseUri", ParseUri);

    target->Set(String::NewSymbol("RaptorParser"), persistent_function_template->GetFunction());
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    RaptorParser* parser_instance = new RaptorParser();

    if (args.Length() < 1) {                   
      return ThrowException(Exception::TypeError(String::New("Argument 1  must be a media type"))); 
    }
    Persistent<String> media_type = Persistent<String>::New(Local<String>::Cast(args[0]));
    if(args.Length() == 2) {
      Persistent<String> base_uri = Persistent<String>::New(Local<String>::Cast(args[1]));
      parser_instance->base_uri = base_uri;
    } else {
      std::string uri_text = std::string("http://rdf_raptor_js.org/default#");
      char* uri_text_array = new char[uri_text.length()];
      strcpy(uri_text_array, uri_text.c_str());
      Persistent<String> base_uri = Persistent<String>::New(String::New(uri_text_array,
                                                                        strlen(uri_text_array)));
      parser_instance->base_uri = base_uri;
    }

    parser_instance->media_type = media_type;

    ev_async_init(parser_instance->triple_parsed_async_, RaptorParser::NewTripleToEmit);
    ev_async_start(EV_DEFAULT_UC_ parser_instance->triple_parsed_async_);
     
    ev_async_init(parser_instance->finished_parsed_async_, RaptorParser::FinishedParsingEmit);
    ev_async_start(EV_DEFAULT_UC_ parser_instance->finished_parsed_async_);
     
    ev_async_init(parser_instance->error_parsing_async_, RaptorParser::ErrorParsingEmit);
    ev_async_start(EV_DEFAULT_UC_ parser_instance->error_parsing_async_);
     
    ev_unref(EV_DEFAULT_UC);
    ev_unref(EV_DEFAULT_UC);

    parser_instance->Wrap(args.This());      
    parser_instance->Ref();

    return args.This();
  }


  static Handle<Value> Parse(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1) {                   
      return ThrowException(Exception::TypeError(String::New("An argument with the text to be parsed must be provided as the first argument"))); 
    }

    RaptorParser* parser_instance = node::ObjectWrap::Unwrap<RaptorParser>(args.This());
    
    parser_instance->kindOfParsing = std::string("memory");

    Local<String> toParseTmp = Local<String>::Cast(args[0]);
    Persistent<String> toParse = Persistent<String>::New(toParseTmp);

    parser_instance->to_parse = toParse;
    pthread_create(&parser_thread, NULL, ParserThread, parser_instance);
    
    return Undefined();

  }

  static Handle<Value> ParseFile(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1) {                   
      return ThrowException(Exception::TypeError(String::New("An argument with the file to be parsed must be provided as the first argument"))); 
    }

    RaptorParser* parser_instance = node::ObjectWrap::Unwrap<RaptorParser>(args.This());
    
    parser_instance->kindOfParsing = std::string("file");

    Local<String> toParseTmp = Local<String>::Cast(args[0]);
    Persistent<String> toParse = Persistent<String>::New(toParseTmp);

    parser_instance->to_parse = toParse;
    pthread_create(&parser_thread, NULL, ParserThread, parser_instance);
    
    return Undefined();

  }

  static Handle<Value> ParseUri(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1) {                   
      return ThrowException(Exception::TypeError(String::New("An argument with the uri linking the content  to be parsed must be provided as the first argument"))); 
    }

    RaptorParser* parser_instance = node::ObjectWrap::Unwrap<RaptorParser>(args.This());
    
    parser_instance->kindOfParsing = std::string("uri");

    Local<String> toParseTmp = Local<String>::Cast(args[0]);
    Persistent<String> toParse = Persistent<String>::New(toParseTmp);

    parser_instance->to_parse = toParse;
    pthread_create(&parser_thread, NULL, ParserThread, parser_instance);
    
    return Undefined();

  }

  static void ErrorParsingEmit(EV_P_ ev_async *w, int revents){
    
    RaptorParser* parser_instance = static_cast<RaptorParser*>(w->data);    
    parser_instance->foundError = true;
    parser_instance->Emit(error_parsing_symbol, 0, NULL);
  }

  static void FinishedParsingEmit(EV_P_ ev_async *w, int revents){
    RaptorParser* parser_instance = static_cast<RaptorParser*>(w->data);

    pthread_mutex_lock(&queue_mutex);
    while(!parser_instance->statements_queue->empty()) {

      triple_struct* triple_info = parser_instance->statements_queue->front();
      parser_instance->statements_queue->pop();

      RaptorParser* parser_instance = parser_instance;
   
      Local<ObjectTemplate> tripleTemplate = ObjectTemplate::New();
      tripleTemplate->Set(String::New("subject"),parseNode(triple_info->subject));
      tripleTemplate->Set(String::New("predicate"),parseNode(triple_info->predicate));
      tripleTemplate->Set(String::New("object"),parseNode(triple_info->object));
   
      delete[](triple_info->object->value);
      delete[](triple_info->object->kind);
      delete[](triple_info->predicate->value);
      delete[](triple_info->predicate->kind);
      delete(triple_info->predicate);
      delete[](triple_info->subject->value);
      delete[](triple_info->subject->kind);
      delete(triple_info->subject);

      
      if(triple_info->object->type != NULL) {
        delete[] triple_info->object->type;
      }
      if(triple_info->object->lang != NULL) {
        delete[] triple_info->object->lang;
      }
      delete(triple_info->object);

      delete(triple_info);
      Handle<Value> argv[1];
      Local<Object> triple  = tripleTemplate->NewInstance();
      argv[0] = triple;

      parser_instance->Emit(parsed_triple_symbol, 1, argv);
      parser_instance->Unref();   

    }
    pthread_mutex_unlock(&queue_mutex);

    parser_instance->Emit(finished_parsing_symbol, 0, NULL);
    ev_unref(EV_DEFAULT_UC);
  }

  static void NewTripleToEmit(EV_P_ ev_async *w, int revents){

    triple_out_data* triple_out = static_cast<triple_out_data*>(w->data);
    RaptorParser* parser_instance = triple_out->parser_instance;

    pthread_mutex_lock(&queue_mutex);
    while(!parser_instance->statements_queue->empty()) {


      triple_struct* triple_info = parser_instance->statements_queue->front();
      parser_instance->statements_queue->pop();
   
      Local<ObjectTemplate> tripleTemplate = ObjectTemplate::New();
      tripleTemplate->Set(String::New("subject"),parseNode(triple_info->subject));
      tripleTemplate->Set(String::New("predicate"),parseNode(triple_info->predicate));
      tripleTemplate->Set(String::New("object"),parseNode(triple_info->object));
   
      delete[](triple_info->object->value);
      delete[](triple_info->object->kind);
      delete[](triple_info->predicate->value);
      delete[](triple_info->predicate->kind);
      delete(triple_info->predicate);
      delete[](triple_info->subject->value);
      delete[](triple_info->subject->kind);
      delete(triple_info->subject);

      
      if(triple_info->object->type != NULL) {
        delete[] triple_info->object->type;
      }
      if(triple_info->object->lang != NULL) {
        delete[] triple_info->object->lang;
      }
      delete(triple_info->object);

      delete(triple_info);
      
      Handle<Value> argv[1];
      argv[0] = tripleTemplate->NewInstance();

      parser_instance->Emit(parsed_triple_symbol, 1, argv);

    }
    pthread_mutex_unlock(&queue_mutex);
    
  }

  static Local<Object> parseNode(node_struct* node) {
    HandleScope scope;

    Local<ObjectTemplate> nodeTemplate = ObjectTemplate::New();
    Local<String> property = String::New("token");
    Local<String> value = String::New((char*)node->kind);
    nodeTemplate->Set(property, value);

    if(strcmp(node->kind, "uri")==0) {
      Local<String> valueProperty = String::New("value");
      Local<String> valueValue = String::New((char*)node->value);

      fflush(stdout);

      nodeTemplate->Set(valueProperty,valueValue);
    } else if(strcmp(node->kind, "blank")==0) {
      Local<String> valueProperty = String::New("value");
      Local<String> valueValue = String::New((char*)node->value);
      nodeTemplate->Set(valueProperty,valueValue);
    } else {
      Local<String> valueProperty = String::New("value");
      Local<String> valueValue = String::New((char*)node->value);
      nodeTemplate->Set(valueProperty,valueValue);
      if(node->type != NULL) {
        Local<String> typeProperty = String::New("type");
        Local<String> typeValue = String::New((char*)node->type);
        nodeTemplate->Set(typeProperty,typeValue);
      } else if(node->lang != NULL) {
        Local<String> langProperty = String::New("lang");
        Local<String> langValue = String::New((char*)node->lang);
        nodeTemplate->Set(langProperty,langValue);
      }
    }

    
    return scope.Close(nodeTemplate->NewInstance());
  }

};


// triples parsing

static node_struct* parseRaptorNode(const void* data, raptor_identifier_type type, raptor_uri* objectLiteralDatatype, const unsigned char* objectLanguage) {

  node_struct* theNode = new node_struct();

  switch( type ) {
  case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
  case RAPTOR_IDENTIFIER_TYPE_PREDICATE:
  case RAPTOR_IDENTIFIER_TYPE_ORDINAL: {
    int lengthToCopy = strlen("uri");
    theNode->kind = new char[lengthToCopy];
    strncpy(theNode->kind,"uri",lengthToCopy);
    theNode->kind[lengthToCopy] = '\0';
    theNode->value = raptor_uri_to_string( (raptor_uri*)data );
    return theNode;
  }
            
  case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS: {
    int lengthToCopy = strlen((char*)data);
    unsigned char* buffer = new unsigned char[lengthToCopy+1];
    strncpy((char*)buffer, (char*)data, lengthToCopy);
    buffer[lengthToCopy] = '\0';
     
    theNode->kind = new char[strlen("blank")];
    strcpy(theNode->kind,"blank");
    theNode->value = buffer;
    return theNode;
  }

  case RAPTOR_IDENTIFIER_TYPE_LITERAL:
  case RAPTOR_IDENTIFIER_TYPE_XML_LITERAL: {
    int lengthToCopy = strlen("literal");
    theNode->kind = new char[lengthToCopy];
    strncpy(theNode->kind,"literal",lengthToCopy);
    if(objectLiteralDatatype != NULL) {
      lengthToCopy = strlen((char*)data);
      unsigned char* buffer = new unsigned char[lengthToCopy+1];
      strncpy((char*)buffer, (char*)data, lengthToCopy);
      buffer[lengthToCopy] = '\0';
      theNode->value = buffer;
       
      theNode->type = (unsigned char*) raptor_uri_to_string( objectLiteralDatatype );
      theNode->lang = NULL;
      return theNode;

    } else if(objectLanguage != NULL) {
      lengthToCopy = strlen((char*)data);
      unsigned char* buffer = new unsigned char[lengthToCopy+1];
      strncpy((char*)buffer, (char*)data, lengthToCopy);
      buffer[lengthToCopy] = '\0';
      theNode->value = buffer;
       
       
      lengthToCopy = strlen((char*)objectLanguage);
      unsigned char* langBuffer = new unsigned char[lengthToCopy+1];
      strncpy((char*)langBuffer, (char*)objectLanguage, lengthToCopy);
      langBuffer[lengthToCopy]='\0';
      theNode->lang = langBuffer ;
      theNode->type = NULL;
      return theNode;
    } else {
      lengthToCopy = strlen((char*)data);
      unsigned char* buffer = new unsigned char[lengthToCopy+1];
      strncpy((char*)buffer, (char*)data, lengthToCopy);
      buffer[lengthToCopy] = '\0';
       
      theNode->value = buffer;
      theNode->type = NULL;
      theNode->lang = NULL;
      return theNode;
    }
  }
  default: {
    return theNode;
  }
  }
}

static void raptorTriplesHandler( void* data, const raptor_statement* triple ) {
  triple_struct* triple_info = new triple_struct();

  triple_info->subject = parseRaptorNode(triple->subject, triple->subject_type, NULL, NULL);
  triple_info->predicate = parseRaptorNode(triple->predicate, triple->predicate_type, NULL, NULL);
  triple_info->object = parseRaptorNode(triple->object, triple->object_type, triple->object_literal_datatype, triple->object_literal_language);

  RaptorParser* parser_instance = static_cast<RaptorParser *>(data);
  RaptorParser::triple_out_data* triple_out = new RaptorParser::triple_out_data();

  triple_out->parser_instance = parser_instance;

  parser_instance->triple_parsed_async_->data = triple_out;

  pthread_mutex_lock(&queue_mutex);
  if(!parser_instance->foundError) {
    counter++;
    parser_instance->statements_queue->push(triple_info);
  }

  pthread_mutex_unlock(&queue_mutex);

  ev_async_send(EV_DEFAULT_UC_ parser_instance->triple_parsed_async_); 
}

// parser thread
static void raptorErrorHandler(void* data, raptor_locator* locator, const char* message ) {
  RaptorParser* parser_instance = static_cast<RaptorParser*>(data);  

  parser_instance->error_parsing_async_->data = parser_instance;

  ev_async_send(EV_DEFAULT_UC_ parser_instance->error_parsing_async_); 
}

static void* ParserThread(void * data) {

  RaptorParser* parser_instance = static_cast<RaptorParser*>(data);
  
  raptor_init();

  raptor_parser* parser = raptor_new_parser_for_content(0, *String::AsciiValue(parser_instance->media_type), 0, 0, 0);

  raptor_set_fatal_error_handler( parser, parser_instance, raptorErrorHandler );      
  raptor_set_error_handler( parser, parser_instance, raptorErrorHandler );      
  raptor_set_warning_handler( parser, parser_instance, raptorErrorHandler );      

  raptor_set_statement_handler( parser, parser_instance, raptorTriplesHandler );
      
  raptor_uri* raptorBaseUri = raptor_new_uri( (unsigned char *)  *String::AsciiValue(parser_instance->base_uri));
  raptor_uri* uriToParse = 0;

  raptor_start_parse( parser, raptorBaseUri );

  if(parser_instance->kindOfParsing.compare("uri")==0) {
    uriToParse = raptor_new_uri( (unsigned char *)  *String::AsciiValue(parser_instance->to_parse) );
    fflush(stdout);

    raptor_parse_uri( parser, 
                      uriToParse,
                      raptorBaseUri );
  } else if(parser_instance->kindOfParsing.compare("file")==0) {
    fflush(stdout);
    FILE* stream=fopen(*String::AsciiValue(parser_instance->to_parse), "rb");
    raptor_parse_file_stream( parser, 
                              stream, 
                              (const char *) *String::AsciiValue(parser_instance->to_parse), 
                              raptorBaseUri );
    fclose(stream);
  } else {
    raptor_parse_chunk( parser, 
                        (unsigned char*) *String::Utf8Value(parser_instance->to_parse),
                        String::Utf8Value(parser_instance->to_parse).length(),
                        0 );
    raptor_parse_chunk( parser, NULL, 0, 1 );
  }
  raptor_free_parser( parser );


  if ( raptorBaseUri ) {
    raptor_free_uri( raptorBaseUri );
  }

  if( uriToParse ) {
    raptor_free_uri( uriToParse );
  }
      
  raptor_finish(); 

  parser_instance->finished_parsed_async_->data = parser_instance;

  ev_async_send(EV_DEFAULT_UC_ parser_instance->finished_parsed_async_); 

  return NULL;
}


Persistent<FunctionTemplate> RaptorParser::persistent_function_template;
extern "C" {
  static void init(Handle<Object> target) {
    RaptorParser::Init(target);
  }

  NODE_MODULE(raptor_parser, init);
}

