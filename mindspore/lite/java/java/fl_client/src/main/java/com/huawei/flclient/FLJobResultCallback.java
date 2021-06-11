/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.huawei.flclient;

import java.util.logging.Logger;

public class FLJobResultCallback  implements IFLJobResultCallback{
    private static final Logger logger = Logger.getLogger(FLJobResultCallback.class.toString());
    public void onFlJobIterationFinished(String modelName, int iterationSeq, int resultCode) {
        logger.info(Common.addTag("[onFlJobIterationFinished] modelName: " + modelName + " iterationSeq: " + iterationSeq + " resultCode: " + resultCode));
    }
    public void onFlJobFinished(String modelName, int iterationCount, int resultCode) {
        logger.info(Common.addTag("[onFlJobFinished] modelName: " + modelName + " iterationCount: " + iterationCount + " resultCode: " + resultCode));
    }

}
