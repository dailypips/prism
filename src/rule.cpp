// Copyright 2013, QT Inc.
// All rights reserved.
//
// Author: wndproc@gmail.com (Ray Ni)
//
// for rules implementation.

#include "rule.h"
#include "asset.h"
#include "indicator.h"
#include "time_series.h"
#include <ctime>
#include <iostream>

namespace prism {

	void Rule::Serialize(JsonSerializer* serializer)
	{
		serializer->String("type");
		serializer->String(Rule::TypeToStr(type_).c_str());
	}

	bool Rule::Parse(JsonValue* json)
	{
		std::string type = json->operator[]("type").GetString();
		assert(type_ == Rule::StrToType(type));
		return true;
	}

	std::string Rule::TypeToStr(RULE_TYPE type)
	{
		switch (type)
		{
		case RULE_TYPE_AND:
			return "GROUP_AND";
		case RULE_TYPE_OR:
			return "GROUP_OR";
		case RULE_TYPE_TRUE:
			return "CONST_TRUE";
		case RULE_TYPE_FALSE:
			return "CONST_FALSE";
		case RULE_TYPE_INDICATOR_EMA:
			return "INDICATOR_EMA";
		case RULE_TYPE_INDICATOR_EMA_COMPARE:
			return "INDICATOR_EMA_COMPARE";
		case RULE_TYPE_INDICATOR_MACD:
			return "INDICATOR_MACD";
		case RULE_TYPE_INDICATOR_EMAARRAY:
			return "INDICATOR_EMAARRAY";
		case RULE_TYPE_INDICATOR_CR:
			return "INDICATOR_CR";
		default:
			break;
		}
		return "";
	}

	RULE_TYPE Rule::StrToType(const std::string& type)
	{
		if (type == "GROUP_AND")
			return RULE_TYPE_AND;
		else if(type == "GROUP_OR")
			return RULE_TYPE_OR;
		else if (type == "CONST_TRUE")
			return RULE_TYPE_TRUE;
		else if (type == "CONST_FALSE")
			return RULE_TYPE_FALSE;
		else if(type == "INDICATOR_EMA")
			return RULE_TYPE_INDICATOR_EMA;
		else if (type == "INDICATOR_EMA_COMPARE")
			return RULE_TYPE_INDICATOR_EMA_COMPARE;
		else if(type == "INDICATOR_MACD")
			return RULE_TYPE_INDICATOR_MACD;
		else if(type == "INDICATOR_EMAARRAY")
			return RULE_TYPE_INDICATOR_EMAARRAY;
		else if (type == "INDICATOR_CR")
			return RULE_TYPE_INDICATOR_CR;
		else
			return RULE_TYPE_NULL;
	}

	TrueRule::TrueRule() : Rule(RULE_TYPE_TRUE)
	{
	}

	FalseRule::FalseRule() : Rule(RULE_TYPE_FALSE)
	{
	}

	RuleGroup::RuleGroup(RULE_TYPE type) : Rule(type)
	{
	}

	RuleGroup::~RuleGroup()
	{
		Clear();
	}

	void RuleGroup::Clear()
	{
		rules_.clear();
	}


	void RuleGroup::Serialize(JsonSerializer* serializer)
	{
		Rule::Serialize(serializer);
		serializer->String("rules");
		serializer->StartArray();
		for (size_t i = 0; i < rules_.size(); ++i)
		{
			serializer->StartObject();
			rules_[i]->Serialize(serializer);
			serializer->EndObject();
		}
		serializer->EndArray();
	}

	bool RuleGroup::Parse(JsonValue* json)
	{
		std::string type = json->operator[]("type").GetString();
		assert(type_ == Rule::StrToType(type));
		JsonValue& jsonRules = json->operator[]("rules");
		assert(jsonRules.IsArray());
	
		for (rapidjson::SizeType i = 0; i < jsonRules.Size(); i++)	// rapidjson uses SizeType instead of size_t.
		{
			assert(jsonRules[i].IsObject());
			auto rule = RuleFactory::CreateRule(&jsonRules[i]);
			assert(rule != nullptr);
			rules_.push_back(rule);
		}
		return true;
	}

	OrGroup::OrGroup() : RuleGroup(RULE_TYPE_OR)
	{
	}

	bool OrGroup::Verify(AssetIndexer& asset_indexer)
	{
		auto cit = rules_.begin();
		bool result = false;
		while (cit != rules_.end())
		{
			result = result || (*cit)->Verify(asset_indexer);
			cit++;
		}
		return result;
	}

	AndGroup::AndGroup() : RuleGroup(RULE_TYPE_AND)
	{
	}

	bool AndGroup::Verify(AssetIndexer& asset_indexer)
	{
		auto cit = rules_.begin();
		bool result = true;
		while (cit != rules_.end())
		{
			result = result && (*cit)->Verify(asset_indexer);
			cit++;
		}
		return result;
	}

	void IndicatorRule::Serialize(JsonSerializer* serializer)
	{
		Rule::Serialize(serializer);
		serializer->String("data_type");
		serializer->Int((int)data_type_);
		serializer->String("data_num");
		serializer->Int(data_num_);
		serializer->String("duration");
		serializer->Int(duration_);
	}

	bool IndicatorRule::Parse(JsonValue* json)
	{
		data_type_ = (DATA_TYPE)json->operator[]("data_type").GetInt();
		data_num_ = json->operator[]("data_num").GetInt();
		duration_ = json->operator[]("duration").GetInt();
		return Rule::Parse(json);
	}

	EMARule::EMARule() : IndicatorRule(RULE_TYPE_INDICATOR_EMA)
	{
	}
	
	EMARule::EMARule(int period) : period_(period), IndicatorRule(RULE_TYPE_INDICATOR_EMA)
	{
	}

	void EMARule::Serialize(JsonSerializer* serializer)
	{
		IndicatorRule::Serialize(serializer);
		serializer->String("period");
		serializer->Int(period_);
	}

	bool EMARule::Parse(JsonValue* json)
	{		
		period_ = json->operator[]("period").GetInt();
		return IndicatorRule::Parse(json);
	}

	bool EMARule::Verify(AssetIndexer& asset_indexer)
	{
		std::string indicator_str = std::string("EMA") + "_" + std::to_string(period_);
		auto scale = asset_indexer.asset()->scales(data_type(), data_num());
		auto indicator = scale->indicators(indicator_str);
		auto ema = (EMA*)(indicator);
		assert(ema != nullptr);
		auto tl = ema->result();
		size_t pos = asset_indexer.scale_indexers(scale)->index();
		int index = pos - period_;
		if (index < 0)
			return false;
		else
		{
		//	std::cout << TimeToString(tl->at(index).position, "time: %Y-%m-%d") << "," << index << "," << indicator_str << ", close:" << asset->raw_data()->at(pos).close << ", value:" << tl->at(index).value << std::endl;
			return tl->at(index).value < scale->raw_data()->at(pos).close;
		}
	}

	EMACompareRule::EMACompareRule() : IndicatorRule(RULE_TYPE_INDICATOR_EMA_COMPARE)
	{
	}

	EMACompareRule::EMACompareRule(int period_one, int period_two) : period_one_(period_one), period_two_(period_two),
		IndicatorRule(RULE_TYPE_INDICATOR_EMA_COMPARE)
	{
	}

	void EMACompareRule::Serialize(JsonSerializer* serializer)
	{
		IndicatorRule::Serialize(serializer);
		serializer->String("period_one");
		serializer->Int(period_one_);
		serializer->String("period_two");
		serializer->Int(period_two_);
	}

	bool EMACompareRule::Parse(JsonValue* json)
	{		
		period_one_ = json->operator[]("period_one").GetInt();
		period_two_ = json->operator[]("period_two").GetInt();
		return IndicatorRule::Parse(json);
	}

	bool EMACompareRule::Verify(AssetIndexer& asset_indexer)
	{
		std::string indicator_str = std::string("EMA") + "_" + std::to_string(period_one_);
		auto scale = asset_indexer.asset()->scales(data_type(), data_num());
		auto ema_one = (EMA*)(scale->indicators(indicator_str));
		indicator_str = std::string("EMA") + "_" + std::to_string(period_two_);
		auto ema_two = (EMA*)(scale->indicators(indicator_str));
		assert(ema_one != nullptr);
		assert(ema_two != nullptr);

		auto tl_one = ema_one->result();
		auto tl_two = ema_two->result();
		
		size_t pos = asset_indexer.scale_indexers(scale)->index();
		int index = pos - std::max(period_one_, period_two_);
		if (index < 0)
			return false;
		else
		{
			return tl_one->at(pos - period_one_).value > tl_two->at(pos - period_two_).value;
		}
	}

	MACDRule::MACDRule() : IndicatorRule(RULE_TYPE_INDICATOR_MACD)
	{
		Init();
	}

	MACDRule::MACDRule(int short_period, int long_period, int signal_period) : 
		short_period_(short_period), long_period_(long_period), signal_period_(signal_period),
		IndicatorRule(RULE_TYPE_INDICATOR_MACD)
	{
		Init();
	}

	void MACDRule::Init()
	{
		linear_predict_ = true;
		look_back_ = kMacdLookBack;
		threshold_ = kMacdThreshold;
		duration_ = -1;
	}

	void MACDRule::Serialize(JsonSerializer* serializer)
	{
		IndicatorRule::Serialize(serializer);
		serializer->String("short_period");
		serializer->Int(short_period_);
		serializer->String("long_period");
		serializer->Int(long_period_);
		serializer->String("signal_period");
		serializer->Int(signal_period_);
		serializer->String("linear_predict");
		serializer->Bool(linear_predict_);
		serializer->String("look_back");
		serializer->Int(look_back_);
		serializer->String("threshold");
		serializer->Double(threshold_);
	}

	bool MACDRule::Parse(JsonValue* json)
	{
		short_period_ = json->operator[]("short_period").GetInt();
		long_period_ = json->operator[]("long_period").GetInt();
		signal_period_ = json->operator[]("signal_period").GetInt();
		linear_predict_ = json->operator[]("linear_predict").GetBool();
		look_back_ = json->operator[]("look_back").GetInt();
		threshold_ = json->operator[]("threshold").GetDouble();

		return IndicatorRule::Parse(json);
	}

	bool MACDRule::Verify(AssetIndexer& asset_indexer)
	{
		std::string indicator_str = std::string("MACD") + "_" + std::to_string(short_period_) + "_" +
			std::to_string(long_period_) + "_" + 
			std::to_string(signal_period_);
		auto scale = asset_indexer.asset()->scales(data_type(), data_num());
		auto macd = (MACD*)(scale->indicators(indicator_str));
		assert(macd != nullptr);
		auto tl = macd->histogram();
		//align the index for HLOC and for indicator...		
		size_t pos = asset_indexer.scale_indexers(scale)->index();
		int index = pos - long_period_ - signal_period_;	
		int macd_size = tl->size();
		assert(index < macd_size);
		if (index < 0)
			return false;
		else
		{
			return CheckDuration(tl, index);
		}
	}

	bool MACDRule::CheckOnePoint(DoubleTimeList* tl, int index)
	{
		//std::cout << TimeToString(tl->at(index).position, "time: %Y-%m-%d") << "," << index << ","<< indicator_str << ", value:" << tl->at(index).value << std::endl;
		double predict_value = tl->at(index).value;
		if (linear_predict_)
		{
			// for now, predict next macd histogram value by linear interpolating
			// TODO: to find a better way to predict next macd value based on historical data... worthwhile researching
			DoubleTimeList fit_points;
			DoubleTimeList result;
			for (int i = 0; i < look_back_; ++i)
			{
				if (index - i >= 0)
				{
					double value = tl->at(index - i).value;
					if (value * predict_value > 0) // same sign with current value
					{
						fit_points.push_back(DoubleTimePoint(value, index - i));
					}
				}
			}
			if (fit_points.size() > 1)
			{
				TimeSeries ts(fit_points.begin(), fit_points.end());
				LRCoef coef = ts.LinearRegression(&result);
				predict_value = coef.A * (index + 1) + coef.B;
			}
		}
		return predict_value > threshold_;
	}

	bool MACDRule::CheckDuration(DoubleTimeList* tl, int start)
	{
		if (duration_ < 0) // ignore duration, only check current point
			return CheckOnePoint(tl, start);
		int i = 0;
		while (start - i >= 0)
		{
			if (i > duration_)
				return false;
			if (CheckOnePoint(tl, start - i))
				i++;
			else
				break;
		}
		return (i > 0) && (i <= duration_);
	}

	EMAArrayRule::EMAArrayRule(): IndicatorRule(RULE_TYPE_INDICATOR_EMAARRAY)
	{
	}

	EMAArrayRule::EMAArrayRule(int first_period, int second_period, int third_period, int fourth_period) :
		first_period_(first_period), second_period_(second_period), third_period_(third_period), fourth_period_(fourth_period),
		IndicatorRule(RULE_TYPE_INDICATOR_EMAARRAY)
	{
	}

	void EMAArrayRule::Serialize(JsonSerializer* serializer)
	{
		IndicatorRule::Serialize(serializer);
		serializer->String("first_period");
		serializer->Int(first_period_);
		serializer->String("second_period");
		serializer->Int(second_period_);
		serializer->String("third_period");
		serializer->Int(third_period_);
		serializer->String("fourth_period");
		serializer->Int(fourth_period_);

	}
	
	bool EMAArrayRule::Parse(JsonValue* json)
	{
		first_period_ = json->operator[]("first_period").GetInt();
		second_period_ = json->operator[]("second_period").GetInt();
		third_period_ = json->operator[]("third_period").GetInt();
		fourth_period_ = json->operator[]("fourth_period").GetInt();

		return IndicatorRule::Parse(json) && first_period_ < second_period_ && second_period_ < third_period_ && third_period_ < fourth_period_;
	}
	
	bool EMAArrayRule::Verify(AssetIndexer& asset_indexer)
	{
		std::string indicator_str1 = std::string("EMA") + "_" + std::to_string(first_period_);
		std::string indicator_str2 = std::string("EMA") + "_" + std::to_string(second_period_);
		std::string indicator_str3 = std::string("EMA") + "_" + std::to_string(third_period_);
		std::string indicator_str4 = std::string("EMA") + "_" + std::to_string(fourth_period_);
		
		auto scale = asset_indexer.asset()->scales(data_type(), data_num());

		auto ema1 = (EMA*)(scale->indicators(indicator_str1));
		auto ema2 = (EMA*)(scale->indicators(indicator_str2));
		auto ema3 = (EMA*)(scale->indicators(indicator_str3));
		auto ema4 = (EMA*)(scale->indicators(indicator_str4));

		assert(ema1 != nullptr);
		assert(ema2 != nullptr);
		assert(ema3 != nullptr);
		assert(ema4 != nullptr);

		auto tl1 = ema1->result();
		auto tl2 = ema2->result();
		auto tl3 = ema3->result();
		auto tl4 = ema4->result();

		size_t pos = asset_indexer.scale_indexers(scale)->index();
		int index = pos - fourth_period_;
		if (index < 0)
			return false;
		else
		{
			double period1_value = tl1->at(pos - first_period_).value;
			double period2_value = tl2->at(pos - second_period_).value;
			double period3_value = tl3->at(pos - third_period_).value;
			double period4_value = tl4->at(pos - fourth_period_).value;
			double close = scale->raw_data()->at(pos).close;
			/*
			std::cout << TimeToString(tl1->at(pos - period1_).position, "time: %Y-%m-%d") 
				<< "," << asset->raw_data()->at(pos).close 
				<< "," << tl1->at(pos - period1_).value 
				<< "," << tl2->at(pos - period2_).value 
				<< "," << tl3->at(pos - period3_).value 
				<< "," << tl4->at(pos - period4_).value 
				<< std::endl;
			*/
			assert( tl1->at(pos - first_period_).position == tl2->at(pos - second_period_).position);
			return period4_value < period3_value
				&& period3_value < period2_value
				&& period2_value < period1_value 
				&& period1_value < close;
		}
	}

	CRRule::CRRule(): IndicatorRule(RULE_TYPE_INDICATOR_CR)
	{
	}

	CRRule::CRRule(int period) : period_(period), IndicatorRule(RULE_TYPE_INDICATOR_CR)
	{
	}

	void CRRule::Serialize(JsonSerializer* serializer)
	{
		IndicatorRule::Serialize(serializer);
		serializer->String("period");
		serializer->Int(period_);
	}

	bool CRRule::Parse(JsonValue* json)
	{
		period_ = json->operator[]("period").GetInt();
		return IndicatorRule::Parse(json);
	}

	bool CRRule::Verify(AssetIndexer& asset_indexer)
	{
		std::string indicator_str = std::string("CR") + "_" + std::to_string(period_);
		auto scale = asset_indexer.asset()->scales(data_type(), data_num());
		auto cr = (CR*)(scale->indicators(indicator_str));
		assert(cr != nullptr);
		auto tl = cr->result();

		size_t pos = asset_indexer.scale_indexers(scale)->index();
		int index = pos;
		if (index < 0)
			return false;
		else
		{
			std::cout << asset_indexer.asset()->symbol() << TimeToString(tl->at(index).position, ", time: %Y-%m-%d, ") << tl->at(index).value << "," << 1.0 / (double)period_<< std::endl;
			return tl->at(index).value <=  1.0 / (double)period_;
		}
	}

	RuleFactory::RuleFactory()
	{
	}

	RuleFactory::~RuleFactory()
	{

	}

	std::shared_ptr<IRule> RuleFactory::CreateRuleType(RULE_TYPE type)
	{
		switch (type)
		{
		case RULE_TYPE_AND:
			return std::make_shared<AndGroup>();
		case RULE_TYPE_OR:
			return std::make_shared<OrGroup>();
		case RULE_TYPE_TRUE:
			return std::make_shared<TrueRule>();
		case RULE_TYPE_FALSE:
			return std::make_shared<FalseRule>();
		case RULE_TYPE_INDICATOR_EMA:
			return std::make_shared<EMARule>();
		case RULE_TYPE_INDICATOR_EMA_COMPARE:
			return std::make_shared<EMACompareRule>();
		case RULE_TYPE_INDICATOR_MACD:
			return std::make_shared<MACDRule>();
		case RULE_TYPE_INDICATOR_EMAARRAY:
			return std::make_shared<EMAArrayRule>();
		case RULE_TYPE_INDICATOR_CR:
			return std::make_shared<CRRule>();
		default:
			break;
		}
		return nullptr;
	}

	std::shared_ptr<IRule> RuleFactory::CreateRule(JsonValue* json)
	{
		assert(json->HasMember("type"));
		std::string type = json->operator[]("type").GetString();
		RULE_TYPE ruleType = Rule::StrToType(type);
		auto rule = CreateRuleType(ruleType);
		assert(rule != nullptr);
		bool ret = rule->Parse(json);
		assert(ret);
		return rule;
	}

}