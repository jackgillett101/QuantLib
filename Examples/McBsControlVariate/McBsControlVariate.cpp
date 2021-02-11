/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#  include <ql/auto_link.hpp>
#endif

#include <ql/instruments/asianoption.hpp>
#include <ql/pricingengines/asian/mc_discr_geom_av_price.hpp>
#include <ql/pricingengines/asian/mc_discr_arith_av_price.hpp>
#include <ql/pricingengines/asian/mc_discr_arith_av_price_heston.hpp>
#include <ql/pricingengines/asian/mc_discr_arith_av_strike.hpp>
#include <ql/pricingengines/asian/mcdiscreteasianenginebase.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <ql/math/integrals/gaussianquadratures.hpp>
#include <iostream>
#include <iomanip>

using namespace QuantLib;

int main(int, char* []) {

    try {

        std::cout << std::endl;
        Date today = Settings::instance().evaluationDate();

        Real strikes[] = {60.0, 80.0, 100.0, 120.0, 140.0};

        DayCounter dc = Actual365Fixed();
        Calendar calendar = NullCalendar();

        Handle<Quote> spotQuote(ext::shared_ptr<Quote>(new SimpleQuote(100.0)));
        Handle<YieldTermStructure> flatTermStructure(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(today, 0.0, dc)));
        Handle<YieldTermStructure> flatDividendTS(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(today, 0.0, dc)));
        ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.25));
        ext::shared_ptr<BlackVolTermStructure> volTS(new BlackConstantVol(today,
                calendar, Handle<Quote>(vol), dc));

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(new
            BlackScholesMertonProcess(spotQuote, flatDividendTS, flatTermStructure,
                                      Handle<BlackVolTermStructure>(volTS)));

        ext::shared_ptr<PricingEngine> engine =
            MakeMCDiscreteArithmeticAPEngine<LowDiscrepancy>(stochProcess)
                .withSamples(32768)
                .withBrownianBridge(false)
                .withControlVariate(false);

        ext::shared_ptr<PricingEngine> engine2 =
            MakeMCDiscreteArithmeticAPEngine<LowDiscrepancy>(stochProcess)
                .withSamples(32768)
                .withBrownianBridge(false)
                .withControlVariate(true);

        Size fixings = 24;

        std::vector<Date> fixingDates(fixings);
        for (Size i=1; i<=fixings; i++) {
            fixingDates[i-1] = today + Period(i, Months);
//            std::cout << "Fixing:        " << fixingDates[i-1] << std::endl;
        }

        ext::shared_ptr<Exercise> exercise(new
            EuropeanExercise(fixingDates[fixings-1]));

//        std::cout << "Expiry:        " << fixingDates[fixings-1] << std::endl;

        Real runningSum = 300.0;
        Size pastFixings = 12;
        std::vector<Real> allPastFixings = {25., 25., 25., 25., 25., 25., 25., 25., 25., 25., 25., 25.};

        for (Size i=0; i<5; i++) {
            Real strike = strikes[i];

            ext::shared_ptr<StrikedTypePayoff> payoff(new
                PlainVanillaPayoff(Option::Call, strike));

            DiscreteAveragingAsianOption option(Average::Arithmetic, 0.0, 0, fixingDates, payoff, exercise);

            DiscreteAveragingAsianOption option2(Average::Arithmetic, fixingDates, payoff, exercise);

            DiscreteAveragingAsianOption option3(Average::Arithmetic, runningSum,
                                                pastFixings, fixingDates, payoff, exercise);

            DiscreteAveragingAsianOption option4(Average::Arithmetic, fixingDates,
                                                 payoff, exercise, allPastFixings);

            std::cout << "  " << std::endl;
            std::cout << "STRIKE: " << strike << std::endl;
            std::cout << "  " << std::endl;
            std::cout << "New-style option without seasoning matches Old-style (with/without CV):" << std::endl;
            std::cout << "  " << std::endl;

            option.setPricingEngine(engine);
            std::cout << "  Original-style unseasoned option, no CV: " << option.NPV() << std::endl;

            option2.setPricingEngine(engine);
            std::cout << "  New-style unseasoned option, no CV:      " << option2.NPV() << std::endl;

            option.setPricingEngine(engine2);
            std::cout << "  Original-style unseasoned option, CV:    " << option.NPV() << std::endl;

            option2.setPricingEngine(engine2);
            std::cout << "  New-style unseasoned option, CV:         " << option2.NPV() << std::endl;

            std::cout << "  " << std::endl;
            std::cout << "New-style option with seasoning and without CV matches Old-style:" << std::endl;
            std::cout << "  " << std::endl;

            option3.setPricingEngine(engine);
            std::cout << "  Original-style seasoned option, no CV:   " << option3.NPV() << std::endl;

            option4.setPricingEngine(engine);
            std::cout << "  New-style seasoned option, no CV:        " << option4.NPV() << std::endl;

            std::cout << "  " << std::endl;
            std::cout << "but Old-style option with seasoning and with CV breaks:" << std::endl;
            std::cout << "  " << std::endl;

            option3.setPricingEngine(engine2);
            std::cout << "  Original-style seasoned option, CV:      " << option3.NPV() << std::endl;

            std::cout << "  " << std::endl;
            std::cout << "Fixed by the New-Style:" << std::endl;
            std::cout << "  " << std::endl;

            option4.setPricingEngine(engine2);
            std::cout << "  New-style seasoned option, CV:           " << option4.NPV() << std::endl;

            std::cout << "  " << std::endl;
        }

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
        return 1;
    }
}

