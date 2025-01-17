// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/serial/serial_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";
const char kCrossOriginTestUrl[] = "https://www.chromium.org";

class SerialTest : public RenderViewHostImplTestHarness {
 public:
  SerialTest() {
    ON_CALL(test_client_.delegate(), GetPortManager)
        .WillByDefault(testing::Return(&port_manager_));
  }

  ~SerialTest() override = default;

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&test_client_);
    RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  device::FakeSerialPortManager* port_manager() { return &port_manager_; }

 private:
  SerialTestContentBrowserClient test_client_;
  ContentBrowserClient* original_client_ = nullptr;
  device::FakeSerialPortManager port_manager_;

  DISALLOW_COPY_AND_ASSIGN(SerialTest);
};

}  // namespace

TEST_F(SerialTest, OpenAndClosePort) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::SerialServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::SerialService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  device::mojom::SerialPortPtr port;
  service->GetPort(token, mojo::MakeRequest(&port));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  port.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
}

TEST_F(SerialTest, OpenAndNavigateCrossOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::SerialServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::SerialService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  device::mojom::SerialPortPtr port;
  service->GetPort(token, mojo::MakeRequest(&port));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  NavigateAndCommit(GURL(kCrossOriginTestUrl));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_TRUE(port.encountered_error());
}

}  // namespace content
