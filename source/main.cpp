#include "app.hpp"

#include "args.hpp"

int main(int argc, char* argv[]) {
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	std::iostream::sync_with_stdio(false);

	args::ArgumentParser parser("");
	args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
	args::ValueFlag<int> port(parser, "port", "Server port", {'p', "port"}, 8081);
	args::ValueFlag<int> width(parser, "width", "Width of the stream output", {'x', "width"}, 0);
	args::ValueFlag<int> height(parser, "height", "Height of the stream output", {'y', "height"}, 0);
	args::ValueFlag<int> bitrate(parser, "bitrate", "Average bitrate of stream output", {'b', "bitrate"}, 0);
	args::ValueFlag<int> fps(parser, "fps", "Target framerate of stream output", {'r', "fps"}, 30);
	args::ValueFlag<int> thread_count(parser, "threads", "Number of threads to use for encoding", {'t', "threads"}, 2);
	args::ValueFlag<int> adapter(parser, "output", "Index of the output adapter", {'o', "output"}, 0);

	try {
		parser.ParseCLI(argc, argv);
	} catch (args::Help) {
		std::stringstream ss;
		std::cout << parser;
		printf_s(ss.str().c_str());
		return 0;
	} catch (args::ParseError e) {
		std::stringstream ss;
		ss << e.what() << std::endl;
		ss << parser;
		fprintf_s(stderr, ss.str().c_str());
		return 1;
	}

	app_t* app = app_create(
		port.Get(),
		bitrate.Get() * 1000,
		fps.Get(),
		thread_count.Get(),
		width.Get(),
		height.Get(),
		adapter.Get());

	if (!app) {
		return 1;
	}

	wprintf_s(
		L"Output size: %dx%d, bitrate: %llu kb/s\n"
		L"Server started on: http://%s:%d/\n",
		app->encoder->out_width,
		app->encoder->out_height,
		app->encoder->context->bit_rate / 1000,
		server_get_host_address(),
		app->server->port);

	app_run(app);
	app_destroy(app);

	return 0;
}
