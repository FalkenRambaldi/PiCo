/*
 This file is part of PiCo.
 PiCo is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 PiCo is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 You should have received a copy of the GNU Lesser General Public License
 along with PiCo.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * FoldReduceBatch.hpp
 *
 *  Created on: Mar 28, 2017
 *      Author: misale
 */

#ifndef INTERNALS_FOLDREDUCEBATCH_HPP_
#define INTERNALS_FOLDREDUCEBATCH_HPP_

#include <ff/node.hpp>

#include "../../Internals/utils.hpp"
#include "../../Internals/Microbatch.hpp"
#include "../../WindowPolicy.hpp"
#include "../ff_config.hpp"
#include "../../Internals/Token.hpp"

using namespace pico;

template<typename In, typename State, typename Farm, typename TokenTypeIn,
		typename TokenTypeState>
class FoldReduceBatch: public Farm {
public:
	FoldReduceBatch(int parallelism,
			std::function<void(const In&, State&)> &foldf_,
			std::function<void(const State&, State&)> &reducef_) {

		this->setEmitterF(
				new ByKeyEmitter<TokenTypeIn>(parallelism, this->getlb(),
						global_params.MICROBATCH_SIZE));
		this->setCollectorF(new Collector(parallelism, reducef_));
		std::vector<ff_node *> w;
		for (int i = 0; i < parallelism; ++i) {
			w.push_back(new Worker(foldf_));
		}
		this->add_workers(w);
		this->cleanup_all();

	}
private:

	class Worker: public base_filter {
	public:
		Worker(std::function<void(const In&, State&)> &foldf_) :
				foldf(foldf_), state(new State()) {

		}

		void kernel(base_microbatch *in_mb) {
			mb_t *in_microbatch = reinterpret_cast<mb_t*>(in_mb);
			tag = in_mb->tag(); //TODO
			// iterate over microbatch
			for (In &in : *in_microbatch) {
				/* build item and enable copy elision */
				foldf(in, *state);
			}
			DELETE(in_microbatch);
		}

		void finalize() {
			/* wrap into a microbatch and send out */
			auto mb = NEW<mb_wrapped<State>>(tag, state);
			ff_send_out(mb);
		}

	private:
		typedef Microbatch<TokenTypeIn> mb_t;
		std::function<void(const In&, State&)> &foldf;
		State* state;

		//TODO tag-partitioned state
		base_microbatch::tag_t tag = 0;
	};

	class Collector: public base_collector {
	public:
		Collector(int nworkers_,
				std::function<void(const State&, State&)> &reducef_) :
				reducef(reducef_) {
		}

		void kernel(base_microbatch *in_mb) {
			auto wmb = reinterpret_cast<mb_wrapped<State> *>(in_mb);
			tag = in_mb->tag(); //TODO
			State* s = reinterpret_cast<State*>(wmb->get());
			reducef(*s, state);
			delete s;
			DELETE(wmb);
		}

		void finalize() {
			auto out_mb = NEW<mb_out>(tag, global_params.MICROBATCH_SIZE);
			new (out_mb->allocate()) State(state);
			out_mb->commit();
			ff_send_out(out_mb);
		}

	private:
		State state;
		std::function<void(const State&, State&)> &reducef;
		typedef Microbatch<TokenTypeState> mb_out;

		//TODO tag-partitioned state
		base_microbatch::tag_t tag = 0;

	};
};

#endif /* INTERNALS_FOLDREDUCEBATCH_HPP_ */
