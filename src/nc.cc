#include <iostream>
#include <iomanip>

#include <DumpFile.hh>

#include <MemoryUtil.hh>

#include <NanoCube.hh>

#include <Query.hh>
#include <QueryParser.hh>

#include <Server.hh>

#include <NanoCubeQueryResult.hh>

#include <NanoCubeSummary.hh>

#include <zlib.h>

using namespace nanocube;

//------------------------------------------------------------------------------
// zlib_compress
//------------------------------------------------------------------------------

std::vector<char> zlib_compress(const char* ptr, std::size_t size)
{
    std::vector<char> result;

    uLongf uncompressed_size = size;             // initially this is an upper bound
    uLongf compressed_size   = compressBound(size); // initially this is an upper bound

    result.resize(compressed_size + 8); // reserve 8 bytes for the original size plus
                                    // enough bytes to fit compressed data

    // write uncompressed size in first 8 bytes of stream
    *reinterpret_cast<uint64_t*>(&result[0]) = (uint64_t) uncompressed_size;

    const Bytef* uncompressed_ptr = reinterpret_cast<const Bytef*>(ptr);
    Bytef*       compressed_ptr   = reinterpret_cast<Bytef*>(&result[8]);

    int status = compress(compressed_ptr, &compressed_size, uncompressed_ptr, uncompressed_size);

    result.resize(compressed_size + 8); // pack

    if (status != Z_OK) {
        throw std::runtime_error("zlib error while compressing");
    }

    return result;
}

//------------------------------------------------------------------------------
// zlib_compress
//------------------------------------------------------------------------------

std::vector<char> zlib_decompress(const char* ptr, std::size_t size)
{
    std::vector<char> result;

    uint64_t aux_uncompressed_size = *reinterpret_cast<const uint64_t*>(ptr);
    uLongf uncompressed_size = (uLongf) aux_uncompressed_size;
    uLongf compressed_size   = size - 8; //

    result.resize(uncompressed_size);  // reserve 8 bytes for the original size

    const Bytef* compressed_ptr   = reinterpret_cast<const Bytef*>(ptr + 8);
    Bytef*       uncompressed_ptr = reinterpret_cast<Bytef*>(&result[0]);

    int status = uncompress(uncompressed_ptr, &uncompressed_size, compressed_ptr, compressed_size);

    // result.resize(output_size + 8);

    if (status != Z_OK) {
        throw std::runtime_error("zlib error while compressing");
    }

    return result;
}




//------------------------------------------------------------------------------
// NanoCube
//------------------------------------------------------------------------------

typedef nanocube::NanoCubeTemplate<
    boost::mpl::vector<LIST_DIMENSION_NAMES>,
    boost::mpl::vector<LIST_VARIABLE_TYPES>> NanoCube;

typedef typename NanoCube::entry_type Entry;

typedef typename NanoCube::address_type Address;

//------------------------------------------------------------------------------
// NanoCubeServer
//------------------------------------------------------------------------------

struct NanoCubeServer {

public: // Constructor

    NanoCubeServer(NanoCube &nanocube);

private: // Private Methods

    void parse(std::string              query_st,
               ::nanocube::Schema        &schema,
               ::query::QueryDescription &query_description);

public: // Public Methods

    void serveQuery     (Request &request, bool json);
    void serveTimeQuery (Request &request, bool json);
    void serveStats     (Request &request);
    void serveSchema    (Request &request);
    void serveTBin      (Request &request);
    void serveSummary   (Request &request);
    void serveGraphViz  (Request &request);

public: // Data Members

    NanoCube &nanocube;
    Server server;

};

//------------------------------------------------------------------------------
// NanoCubeServer Impl.
//------------------------------------------------------------------------------

NanoCubeServer::NanoCubeServer(NanoCube &nanocube):
    nanocube(nanocube)
{
    server.port = 29512;


    bool json = true;
    bool binary  = false;

    auto json_query_handler    = std::bind(&NanoCubeServer::serveQuery, this, std::placeholders::_1, json);
    auto binary_query_handler  = std::bind(&NanoCubeServer::serveQuery, this, std::placeholders::_1, binary);

    auto json_tquery_handler   = std::bind(&NanoCubeServer::serveTimeQuery, this, std::placeholders::_1, json);
    auto binary_tquery_handler = std::bind(&NanoCubeServer::serveTimeQuery, this, std::placeholders::_1, binary);

    auto stats_handler         = std::bind(&NanoCubeServer::serveStats, this, std::placeholders::_1);
    auto schema_handler        = std::bind(&NanoCubeServer::serveSchema, this, std::placeholders::_1);


    auto tbin_handler          = std::bind(&NanoCubeServer::serveTBin, this, std::placeholders::_1);

    auto summary_handler       = std::bind(&NanoCubeServer::serveSummary, this, std::placeholders::_1);

    auto graphviz_handler       = std::bind(&NanoCubeServer::serveGraphViz, this, std::placeholders::_1);

    // register service
    server.registerHandler("query",     json_query_handler);
    server.registerHandler("binquery",  binary_query_handler);
    server.registerHandler("tquery",    json_tquery_handler);
    server.registerHandler("bintquery", binary_tquery_handler);
    server.registerHandler("stats",     stats_handler);
    server.registerHandler("schema",    schema_handler);
    server.registerHandler("tbin",      tbin_handler);
    server.registerHandler("summary",   summary_handler);
    server.registerHandler("graphviz",  graphviz_handler);

    int tentative=0;
    while (tentative < 100) {
        tentative++;
        try {
            std::cout << "Starting NanoCubeServer on port " << server.port << std::endl;
            server.start(10);
        }
        catch (ServerException &e) {
            std::cout << e.what() << std::endl;
            server.port++;
        }
    }

}

void NanoCubeServer::parse(std::string              query_st,
                           ::nanocube::Schema        &schema,
                           ::query::QueryDescription &query_description)
{
    ::query::parser::QueryParser parser;
    parser.parse(query_st);

    // std::cout << ::query::parser::Print(parser) << std::endl;


    for (::query::parser::Dimension *dimension: parser.dimensions) {
        try {
            int index = schema.getDimensionIndex(dimension->name);
            // std::cout << "Index of dimension " << dimension->name << " is " << index << std::endl;

            bool anchored = dimension->anchored;

            query_description.setAnchor(index, anchored);

            ::query::parser::TargetExpression *target = dimension->target;

            target->updateQueryDescription(index, query_description);
        }
        catch(dumpfile::DumpFileException &e) {
            std::cout << "(Warning) Dimension " << dimension->name << " not found. Disconsidering it." << std::endl;
            continue;
        }
    }
}

void NanoCubeServer::serveQuery(Request &request, bool json)
{

    // first entry in params is the url (empty right now)
    // second entry is the handler key, starting from
    // index 2 are the parameters;
    std::stringstream ss;
    bool first = true;
    for (auto it=request.params.begin()+2; it!=request.params.end(); it++) {
        if (!first) {
            ss << "/";
        }
        ss << *it;
        first = false;
    }

    // query string
    // std::string query_st = "src=[qaddr(0,0,2),qaddr(0,0,2)]/@dst=qaddr(1,0,1)+1";
    std::string query_st = ss.str();

    // log
    // std::cout << query_st << std::endl;

    ::query::QueryDescription query_description;

    try {
        this->parse(query_st, nanocube.schema, query_description);
    }
    catch (::query::parser::QueryParserException &e) {
        request.respondJson(e.what());
        return;
    }

    // count number of anchored flags
    int num_anchored_dimensions = 0;
    for (auto flag: query_description.anchors) {
        if (flag)
            num_anchored_dimensions++;
    }

    ::query::result::Vector result_vector(num_anchored_dimensions);

    // set dimension names
    int level=0;
    int i=0;
    for (auto flag: query_description.anchors) {
        if (flag) {
            result_vector.setLevelName(level++, nanocube.schema.getDimensionName(i));
        }
        i++;
    }

    ::query::result::Result result(result_vector);

    // ::query::result::Result result;
    try {
        nanocube.query(query_description, result);
    }
    catch (::nanocube::query::QueryException &e) {
        request.respondJson(e.what());
        return;
    }
    catch (...) {
        request.respondJson("Unexpected problem. Server might be unstable now.");
        return;
    }



    if (json) { // json query

        ::nanocube::QueryResult query_result(result_vector, nanocube.schema);
        ss.str("");
        query_result.json(ss);
        // ss << result;
        request.respondJson(ss.str());

    }
    else { // binary query

        std::ostringstream os;
        ::query::result::serialize(result_vector,os);
        const std::string st = os.str();

        // compress data
        auto compressed_data = zlib_compress(st.c_str(), st.size());
        request.respondOctetStream(&compressed_data[0], compressed_data.size());

        // request.respondOctetStream(st.c_str(), st.size());
    }
}


void NanoCubeServer::serveTimeQuery(Request &request, bool json)
{

    // first entry in params is the url (empty right now)
    // second entry is the handler key, starting from
    // index 2 are the parameters;
    std::stringstream ss;
    bool first = true;
    for (auto it=request.params.begin()+2; it!=request.params.end(); it++) {
        if (!first) {
            ss << "/";
        }
        ss << *it;
        first = false;
    }

    // query string
    // std::string query_st = "src=[qaddr(0,0,2),qaddr(0,0,2)]/@dst=qaddr(1,0,1)+1";
    std::string query_st = ss.str();

    // log
    // std::cout << query_st << std::endl;

    ::query::QueryDescription query_description;

    try {
        this->parse(query_st, nanocube.schema, query_description);
    }
    catch (::query::parser::QueryParserException &e) {
        request.respondJson(e.what());
        return;
    }

    int time_dimension = nanocube.schema.dump_file_description.getTimeFieldIndex();
    if (query_description.anchors[time_dimension] ||
            query_description.targets[time_dimension]->type != ::query::Target::ROOT) {
        request.respondJson("time queries should not constrain time dimension");
        return;
    }

    // set
    query_description.anchors[time_dimension] = true;


    // count number of anchored flags
    int num_anchored_dimensions = 0;
    for (auto flag: query_description.anchors) {
        if (flag)
            ++num_anchored_dimensions;
    }

    ::query::result::Vector result_vector(num_anchored_dimensions);

    // set dimension names
    int level=0;
    int i=0;
    for (auto flag: query_description.anchors) {
        if (flag) {
            result_vector.setLevelName(level++, nanocube.schema.getDimensionName(i));
        }
        i++;
    }

    ::query::result::Result result(result_vector);

    // ::query::result::Result result;
    try {
        nanocube.timeQuery(query_description, result);
    }
    catch (::nanocube::query::QueryException &e) {
        request.respondJson(e.what());
        return;
    }
    catch (...) {
        request.respondJson("Unexpected problem. Server might be unstable now.");
        return;
    }



    if (json) { // json query

        ::nanocube::QueryResult query_result(result_vector, nanocube.schema);
        ss.str("");
        query_result.json(ss);
        // ss << result;
        request.respondJson(ss.str());

    }
    else { // binary query

        std::ostringstream os;
        ::query::result::serialize(result_vector,os);
        const std::string st = os.str();

        // compress data
        auto compressed_data = zlib_compress(st.c_str(), st.size());
        request.respondOctetStream(&compressed_data[0], compressed_data.size());

        // request.respondOctetStream(st.c_str(), st.size());

    }
}

void NanoCubeServer::serveStats(Request &request)
{
    report::Report report(nanocube.DIMENSION + 1);
    nanocube.mountReport(report);
    std::stringstream ss;
    for (auto layer: report.layers) {
        ss << *layer << std::endl;
    }

    request.respondJson(ss.str());
}

void NanoCubeServer::serveGraphViz(Request &request)
{
    report::Report report(nanocube.DIMENSION + 1);
    nanocube.mountReport(report);
    std::stringstream ss;
    report_graphviz(ss, report);
    request.respondJson(ss.str());
}

void NanoCubeServer::serveSchema(Request &request)
{
    // report::Report report(nanocube.DIMENSION + 1);
    // nanocube.mountReport(report);
    std::stringstream ss;
    ss << nanocube.schema.dump_file_description;
    request.respondJson(ss.str());
}


void NanoCubeServer::serveTBin(Request &request)
{
    auto &metadata = nanocube.schema.dump_file_description.metadata;
    auto it = metadata.find("tbin");
    if (it != metadata.end()) {
        auto str = it->second;
        request.respondJson(str);
    }
}

void NanoCubeServer::serveSummary(Request &request)
{
    nanocube::Summary summary = mountSummary(this->nanocube);
    std::stringstream ss;
    ss << summary;
    request.respondJson(ss.str());
}


//-----------------------------------------------------------------------------
// Split routines
//-----------------------------------------------------------------------------

std::vector<std::string> &split(const std::string &s, char delim,
                                std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
         elems.push_back(item);
    }
    return elems;
}

//-----------------------------------------------------------------------------
// Options
//-----------------------------------------------------------------------------

struct Options {
    Options(const std::vector<std::string> &argv);

    uint64_t max_points;
    uint64_t report_frequency;
};

Options::Options(const std::vector<std::string> &argv):
    max_points(0),
    report_frequency(100000)
{
    // read options
    // find type of dump
    try {
        for (std::string param: argv) {
            std::vector<std::string> tokens;
            split(param,'=',tokens);
            if (tokens.size() < 1) {
                continue;
            }
            if (tokens[0].compare("--max") == 0) {
                max_points = std::stoull(tokens[1]);
            }
            if (tokens[0].compare("--report-frequency") == 0 || tokens[0].compare("--rf") == 0) { // report frequency
                report_frequency = std::stoull(tokens[1]);
            }
        }
    }
    catch (...)
    {}
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(int argc, char *argv[]) {

    std::vector<std::string> params;
    for (int i=1;i<argc;i++) {
        params.push_back(argv[i]);
    }

    Options options(params);

    // read input file description
    dumpfile::DumpFileDescription input_file_description;
    std::cin >> input_file_description;

    // create nanocube_schema from input_file_description
    ::nanocube::Schema nanocube_schema(input_file_description);

    // create nanocube
    NanoCube nanocube(nanocube_schema);

    stopwatch::Stopwatch sw;
    sw.start();

    uint64_t count = 0;
    std::cout << "read data now..." << std::endl;
    while (1) {

//        std::cout << "count: " << count << std::endl;

        if (options.max_points > 0 && count == options.max_points) {
            break;
        }

        bool ok = nanocube.add(std::cin);
        if (!ok) {
            // std::cout << "not ok" << std::endl;
            break;
        }

//        { // generate pdf
//            report::Report report(nanocube.DIMENSION + 1);
//            nanocube.mountReport(report);
//            std::string filename     = "/tmp/bug"+std::to_string(count)+".dot";
//            std::string pdf_filename = "/tmp/bug"+std::to_string(count)+".pdf";
//            std::ofstream of(filename);
//            report::report_graphviz(of, report);
//            of.close();
//            system(("dot -Tpdf " + filename + " > " + pdf_filename).c_str());
//        }

        // std::cout << "ok" << std::endl;
        count++;


        if (count % options.report_frequency == 0) {
            std::cout << "count: " << std::setw(10) << count << " mem. res: " << std::setw(10) << memory_util::MemInfo::get().res_MB() << "MB.  time(s): " <<  std::setw(10) << sw.timeInSeconds() << std::endl;
        }


    } // loop to insert objects into the nanocube

    std::cout << "count: " << std::setw(10) << count << " mem. res: " << std::setw(10) << memory_util::MemInfo::get().res_MB() << "MB.  time(s): " << std::setw(10) << sw.timeInSeconds() << std::endl;
    // std::cout << "count: " << count << " mem. res: " << memory_util::MemInfo::get().res_MB() << "MB." << std::endl;

    // test query using query language
    std::cout << "Number of points inserted " << count << std::endl;

    // start nanocube http server
    NanoCubeServer nanocube_server(nanocube);

    return 0;

}
