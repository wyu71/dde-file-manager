/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "event/sequence/eventsequence.h"
#include "testqobject.h"
#include "framework.h"

#include <gtest/gtest.h>

DPF_USE_NAMESPACE

class UT_EventSequence : public testing::Test
{
public:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
    }
};

TEST_F(UT_EventSequence, test_append)
{
    TestQObject b;

    EventSequence e;
    int called { 0 };

    // NOTE!!
    // return value must is bool, otherwise compier failed
    // e.append(&b, &TestQObject::test1);

    e.append(&b, &TestQObject::bigger10);
    e.append(&b, &TestQObject::bigger15);

    EXPECT_FALSE(e.traversal(9, &called));
    EXPECT_EQ(15, called);
    EXPECT_TRUE(e.traversal(11, &called));
    EXPECT_EQ(10, called);
}

TEST_F(UT_EventSequence, test_append_empty_1)
{
    TestQObject b;

    EventSequence e;
    e.append(&b, &TestQObject::empty1);
    e.append(&b, &TestQObject::empty2);
    EXPECT_TRUE(e.traversal());
}

TEST_F(UT_EventSequence, test_append_empty_2)
{
    TestQObject b;

    EventSequence e;
    e.append(&b, &TestQObject::empty2);
    e.append(&b, &TestQObject::empty1);
    EXPECT_TRUE(e.traversal());
}

TEST_F(UT_EventSequence, test_manager)
{
    TestQObject b;
    EventType eType1 = 1;
    EventType eType2 = 2;
    int called { 0 };

    EventSequenceManager::instance().follow(eType1, &b, &TestQObject::bigger15);
    EventSequenceManager::instance().follow(eType1, &b, &TestQObject::bigger10);
    EXPECT_FALSE(EventSequenceManager::instance().run(eType1, 0, &called));
    EXPECT_EQ(10, called);

    EXPECT_TRUE(EventSequenceManager::instance().run(eType1, 16, &called));
    EXPECT_EQ(15, called);

    EXPECT_TRUE(EventSequenceManager::instance().run(eType1, 14, &called));
    EXPECT_EQ(10, called);

    EventSequenceManager::instance().follow(eType2, &b, &TestQObject::bigger10);
    EventSequenceManager::instance().follow(eType2, &b, &TestQObject::bigger15);
    EXPECT_FALSE(EventSequenceManager::instance().run(eType2, 0, &called));
    EXPECT_EQ(15, called);

    EXPECT_TRUE(EventSequenceManager::instance().run(eType2, 16, &called));
    EXPECT_EQ(10, called);

    EXPECT_TRUE(EventSequenceManager::instance().run(eType2, 14, &called));
    EXPECT_EQ(10, called);
}
