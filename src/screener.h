// Copyright 2013, QT Inc.
// All rights reserved.
//
// Author: wndproc@gmail.com (Ray Ni)
//
// routines and classes for screen assets based on rule

#ifndef SCREENER_H
#define SCREENER_H

#include "rule.h"
#include "asset.h"
#include <string>
#include <vector>

namespace prism {
	
	class Screener
	{
	public:
		Screener() { rule_ = nullptr; }
		Screener(const std::shared_ptr<IRule>& rule) : rule_(rule) {}
		~Screener() {}
	public:
		void Screen(AssetIndexerList& asset_index_list, std::vector<int> *result, time_t pos = -1);
		void set_rule(const std::shared_ptr<IRule>& rule) { rule_ = rule; }
	private:
		std::shared_ptr<IRule> rule_;
	};


}

#endif