/*
 * read_from_stdin.cpp
 *
 *  Created on: Oct 19, 2018
 *      Author: martinelli
 */

#include <iostream>
#include <string>

#include <pico/pico.hpp>

#include <catch.hpp>

#include "common/io.hpp"

using namespace pico;


TEST_CASE( "read from stdin and write", "read from stdin and write tag" ){

	std::string input_file = "./testdata/lines.txt";
	std::string output_file = "output.txt";

	/* redirect input file to stdin */
    auto cinbuf = std::cin.rdbuf(); //save old buf
    std::ifstream in(input_file);
    std::cin.rdbuf(in.rdbuf()); //redirect

    /* redirect stdout to output file */
    auto coutbuf = std::cout.rdbuf(); //save old buf
    std::ofstream out(output_file);
    std::cout.rdbuf(out.rdbuf()); //redirect

	/* define i/o operators from/to file */
	ReadFromStdIn reader('\n');
	WriteToStdOut<std::string> writer;

	/* compose the pipeline */
	auto io_file_pipe = Pipe() //the empty pipeline
	.add(reader)
	.add(writer);

	io_file_pipe.run();

    /* undo redirection */
    std::cout.rdbuf(coutbuf);
    std::cin.rdbuf(cinbuf);

	/* forget the order and compare */
	auto input_lines = read_lines(input_file);
	auto output_lines = read_lines(output_file);
	std::sort(input_lines.begin(), input_lines.end());
	std::sort(output_lines.begin(), output_lines.end());

	REQUIRE(input_lines == output_lines);
}






