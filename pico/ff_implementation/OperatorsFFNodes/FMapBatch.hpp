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
 * FMapBatch.hpp.hpp
 *
 *  Created on: Jan 2, 2017
 *      Author: misale
 */

#ifndef INTERNALS_FFOPERATORS_FMAPBATCH_HPP_
#define INTERNALS_FFOPERATORS_FMAPBATCH_HPP_

#include <ff/farm.hpp>

#include "../../Internals/utils.hpp"
#include "../../Internals/TimedToken.hpp"
#include "../../FlatMapCollector.hpp"

using namespace ff;
using namespace pico;

template<typename In, typename Out, typename Farm, typename TokenTypeIn,
		typename TokenTypeOut>
class FMapBatch: public Farm {
public:

	FMapBatch(int parallelism,
			std::function<void(In&, FlatMapCollector<Out> &)> flatmapf) {
		auto e = new ForwardingEmitter<typename Farm::lb_t>(this->getlb());
		auto c = new UnpackingCollector<TokenCollector<Out>>(parallelism);
		this->setEmitterF(e);
		this->setCollectorF(c);
		std::vector<ff_node *> w;
		for (int i = 0; i < parallelism; ++i)
			w.push_back(new Worker(flatmapf));
		this->add_workers(w);
		this->cleanup_all();
	}

private:

	class Worker: public ff_node {
	public:
		Worker(std::function<void(In&, FlatMapCollector<Out> &)> kernel_) :
				kernel(kernel_) {
		}

		void* svc(void* task) {
			if (task != PICO_EOS && task != PICO_SYNC) {
				auto in_microbatch =
						reinterpret_cast<Microbatch<TokenTypeIn>*>(task);
				// iterate over microbatch
				for (In &tt : *in_microbatch) {
					kernel(tt, collector);
				}
				if (collector.begin())
					ff_send_out(reinterpret_cast<void*>(collector.begin()));

				//clean up
				DELETE(in_microbatch, Microbatch<TokenTypeIn>);
				collector.clear();

				return GO_ON;
			} else {
#ifdef DEBUG
				fprintf(stderr, "[UNARYFLATMAP-MB-FFNODE-%p] In SVC SENDING PICO_EOS \n", this);
#endif
				ff_send_out(task);
			}
			return GO_ON; //never reached
		}

	private:
		TokenCollector<Out> collector;
		std::function<void(In&, FlatMapCollector<Out> &)> kernel;
	};
};

template<typename In, typename Out, typename TokenIn, typename TokenOut>
using FMapBatchStream = FMapBatch<In, Out, OrderingFarm, TokenIn, TokenOut>;

template<typename In, typename Out, typename TokenIn, typename TokenOut>
using FMapBatchBag = FMapBatch<In, Out, NonOrderingFarm, TokenIn, TokenOut>;

#endif /* INTERNALS_FFOPERATORS_FMAPBATCH_HPP_ */
