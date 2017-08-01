/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "simulator/impl/simulator.hpp"
#include <gmock/gmock.h>
#include "common/test_subscriber.hpp"

using namespace iroha;
using namespace common::test_subscriber;

using ::testing::Return;
using ::testing::_;

class TemporaryFactoryMock : public ametsuchi::TemporaryFactory {
 public:
  MOCK_METHOD0(createTemporaryWsv, std::unique_ptr<ametsuchi::TemporaryWsv>());
};

class StatefulValidatorMock : public validation::StatefulValidator {
 public:
  MOCK_METHOD2(validate, model::Proposal(const model::Proposal&,
                                         ametsuchi::TemporaryWsv&));
};

/**
 * Mock class for Ametsuchi queries on blocks, transactions
 */
class BlockQueryMock : public ametsuchi::BlockQuery {
 public:
  MOCK_METHOD1(getAccountTransactions,
               rxcpp::observable<iroha::model::Transaction>(std::string));

  MOCK_METHOD2(getBlocks,
               rxcpp::observable<iroha::model::Block>(uint32_t, uint32_t));
};

TEST(SimulatorTest, ValidWhenPreviousBlock) {
  StatefulValidatorMock validator;
  TemporaryFactoryMock factory;
  BlockQueryMock query;
  model::HashProviderImpl provider;

  auto simulator = simulator::Simulator(validator, factory, query, provider);

  auto txs = std::vector<model::Transaction>(2);
  auto proposal = model::Proposal(txs);
  proposal.height = 2;

  model::Block block;
  block.height = proposal.height - 1;

  EXPECT_CALL(factory, createTemporaryWsv()).Times(1);

  EXPECT_CALL(query, getBlocks(proposal.height - 1, proposal.height))
      .WillOnce(Return(rxcpp::observable<>::just(block)));

  EXPECT_CALL(validator, validate(_, _)).WillOnce(Return(proposal));

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator.on_verified_proposal(), 1);
  proposal_wrapper.subscribe([&proposal](auto verified_proposal) {
    ASSERT_EQ(verified_proposal.height, proposal.height);
    ASSERT_EQ(verified_proposal.transactions, proposal.transactions);
  });

  auto block_wrapper = make_test_subscriber<CallExact>(simulator.on_block(), 1);
  block_wrapper.subscribe([&proposal](auto block) {
    ASSERT_EQ(block.height, proposal.height);
    ASSERT_EQ(block.transactions, proposal.transactions);
  });

  simulator.process_proposal(proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}

TEST(SimulatorTest, FailWhenNoBlock) {
  StatefulValidatorMock validator;
  TemporaryFactoryMock factory;
  BlockQueryMock query;
  model::HashProviderImpl provider;

  auto simulator = simulator::Simulator(validator, factory, query, provider);

  auto txs = std::vector<model::Transaction>(2);
  auto proposal = model::Proposal(txs);
  proposal.height = 2;

  EXPECT_CALL(factory, createTemporaryWsv()).Times(0);

  EXPECT_CALL(query, getBlocks(proposal.height - 1, proposal.height))
      .WillOnce(Return(rxcpp::observable<>::empty<model::Block>()));

  EXPECT_CALL(validator, validate(_, _)).Times(0);

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator.on_verified_proposal(), 0);
  proposal_wrapper.subscribe();

  auto block_wrapper = make_test_subscriber<CallExact>(simulator.on_block(), 0);
  block_wrapper.subscribe();

  simulator.process_proposal(proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}

TEST(SimulatorTest, FailWhenSameAsProposalHeight) {
  StatefulValidatorMock validator;
  TemporaryFactoryMock factory;
  BlockQueryMock query;
  model::HashProviderImpl provider;

  auto simulator = simulator::Simulator(validator, factory, query, provider);

  auto txs = std::vector<model::Transaction>(2);
  auto proposal = model::Proposal(txs);
  proposal.height = 2;

  model::Block block;
  block.height = proposal.height;

  EXPECT_CALL(factory, createTemporaryWsv()).Times(0);

  EXPECT_CALL(query, getBlocks(proposal.height - 1, proposal.height))
      .WillOnce(Return(rxcpp::observable<>::just(block)));

  EXPECT_CALL(validator, validate(_, _)).Times(0);

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator.on_verified_proposal(), 0);
  proposal_wrapper.subscribe();

  auto block_wrapper = make_test_subscriber<CallExact>(simulator.on_block(), 0);
  block_wrapper.subscribe();

  simulator.process_proposal(proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}
