#pragma once

#include "EncoderLib/EncStage.h"
#include "Utilities/MsgLog.h"
#include "HWPreAnalyzer.h"

namespace vvenc {

class HWPreAnalysisStage : public EncStage
{
public:
  explicit HWPreAnalysisStage(MsgLog& pcMsg, HWPreAnalyzer* pcAnalyzer);
  virtual ~HWPreAnalysisStage();

  void initPicture(Picture* pic) override;
  void processPictures(const PicList& picList,
                       AccessUnitList& auList,
                       PicList& doneList,
                       PicList& freeList) override;

private:
  int xAttachMetadata(Picture* pic);

  HWPreAnalyzer*  m_pAnalyzer;
  MsgLog&         m_msg;
  int             m_iWarnedMissing;
};

} // namespace vvenc
