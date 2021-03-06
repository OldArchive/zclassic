// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "main.h"

#include "test/test_bitcoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

const CAmount INITIAL_SUBSIDY = 12.5 * COIN;

static int GetTotalHalvings(const Consensus::Params& consensusParams) {
    // This assumes that BUTTERCUP_POW_TARGET_SPACING_RATIO == 2
    // and treats buttercup activation as a halving event
    return consensusParams.vUpgrades[Consensus::UPGRADE_BUTTERCUP].nActivationHeight == Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT ? 64 : 65;
}

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    bool buttercupActive = false;
    int buttercupActivationHeight = consensusParams.vUpgrades[Consensus::UPGRADE_BUTTERCUP].nActivationHeight;
    int nHeight = consensusParams.nSubsidySlowStartInterval;
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), INITIAL_SUBSIDY);
    CAmount nPreviousSubsidy = INITIAL_SUBSIDY;
    for (int nHalvings = 1; nHalvings < GetTotalHalvings(consensusParams); nHalvings++) {
        if (buttercupActive) {
            if (nHeight == buttercupActivationHeight) {
                int preButtercupHeight = (nHalvings - 1) * consensusParams.nPreButtercupSubsidyHalvingInterval + consensusParams.SubsidySlowStartShift();
                nHeight += (preButtercupHeight - buttercupActivationHeight) * Consensus::BUTTERCUP_POW_TARGET_SPACING_RATIO;
            } else {
                nHeight += consensusParams.nPostButtercupSubsidyHalvingInterval;
            }
        } else {
            nHeight = nHalvings * consensusParams.nPreButtercupSubsidyHalvingInterval + consensusParams.SubsidySlowStartShift();
            if (consensusParams.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BUTTERCUP)) {
                nHeight = buttercupActivationHeight;
                buttercupActive = true;
            }
        }
        BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight - 1, consensusParams), nPreviousSubsidy);
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= INITIAL_SUBSIDY);
        BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), 0);
}

static void TestBlockSubsidyHalvings(int nSubsidySlowStartInterval, int nPreButtercupSubsidyHalvingInterval, int buttercupActivationHeight)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidySlowStartInterval = nSubsidySlowStartInterval;
    consensusParams.nPreButtercupSubsidyHalvingInterval = nPreButtercupSubsidyHalvingInterval;
    consensusParams.nPostButtercupSubsidyHalvingInterval = nPreButtercupSubsidyHalvingInterval * Consensus::BUTTERCUP_POW_TARGET_SPACING_RATIO;
    consensusParams.vUpgrades[Consensus::UPGRADE_BUTTERCUP].nActivationHeight = buttercupActivationHeight;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    TestBlockSubsidyHalvings(Params(CBaseChainParams::MAIN).GetConsensus()); // As in main
    TestBlockSubsidyHalvings(20000, Consensus::PRE_BUTTERCUP_HALVING_INTERVAL, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT); // Pre-Buttercup
    TestBlockSubsidyHalvings(50, 150, 80); // As in regtest
    TestBlockSubsidyHalvings(500, 1000, 900); // Just another interval
    TestBlockSubsidyHalvings(500, 1000, 3000); // Multiple halvings before Buttercup activation
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const Consensus::Params& consensusParams = Params(CBaseChainParams::MAIN).GetConsensus();
    
    CAmount nSum = 0;
    int nHeight = 0;
    // Mining slow start
    for (; nHeight < consensusParams.nSubsidySlowStartInterval; nHeight++) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= INITIAL_SUBSIDY);
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, 1250000000);

    // Regular mining
    CAmount nSubsidy;
    do {
        nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= INITIAL_SUBSIDY);
        nSum += nSubsidy;
        BOOST_ASSERT(MoneyRange(nSum));
        ++nHeight;
    } while (nSubsidy > 0);

    // Changing the block interval from 10 to 2.5 minutes causes truncation
    // effects to occur earlier (from the 9th halving interval instead of the
    // 11th), decreasing the total monetary supply by 0.0693 ZEC. If the
    // transaction output field is widened, this discrepancy will become smaller
    // or disappear entirely.
    //BOOST_CHECK_EQUAL(nSum, 2099999997690000ULL);
    // Reducing the interval further to 1.25 minutes has a similar effect,
    // decreasing the total monetary supply by another 0.09240 ZEC.
    // TODO Change this assert when setting the blossom activation height
    // Note that these numbers may or may not change depending on the activation height
    BOOST_CHECK_EQUAL(nSum, 2099999990760000ULL);
    // BOOST_CHECK_EQUAL(nSum, 2099999981520000LL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}

BOOST_AUTO_TEST_SUITE_END()
