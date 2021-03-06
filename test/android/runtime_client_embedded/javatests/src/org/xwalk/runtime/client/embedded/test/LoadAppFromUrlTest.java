// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.xwalk.runtime.client.embedded.test;

import android.test.suitebuilder.annotation.SmallTest;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for loadAppFromUrl().
 */
public class LoadAppFromUrlTest extends XWalkRuntimeClientTestBase {

    @SmallTest
    @Feature({"LoadAppFromUrl"})
    public void testLoadAppFromUrl() throws Throwable {
        String expectedTitle = "Crosswalk Sample Application";

        loadUrlSync("file:///android_asset/index.html");

        String title = getRuntimeView().getTitleForTest();
        assertEquals(expectedTitle, title);
    }
}