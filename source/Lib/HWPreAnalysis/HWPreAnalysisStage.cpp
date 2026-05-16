#include "HWPreAnalysisStage.h"

namespace vvenc {

HWPreAnalysisStage::HWPreAnalysisStage(MsgLog& pcMsg, HWPreAnalyzer* pcAnalyzer)
  : m_pAnalyzer(pcAnalyzer)
  , m_msg(pcMsg)
  , m_iWarnedMissing(0)
{
}

HWPreAnalysisStage::~HWPreAnalysisStage()
{
}

void HWPreAnalysisStage::initPicture(Picture* pic)
{
  xAttachMetadata(pic);
}

void HWPreAnalysisStage::processPictures(const PicList& picList,
                                          AccessUnitList&,
                                          PicList& doneList,
                                          PicList&)
{
  for (auto pic : picList)
    doneList.push_back(pic);
}

int HWPreAnalysisStage::xAttachMetadata(Picture* pic)
{
  if (!m_pAnalyzer || !pic)
    return 1;

  const HWFrameMetadata* pMeta = nullptr;
  int ret = m_pAnalyzer->getFrameMetadata(pic->poc, pMeta);
  if (ret == 0 && pMeta)
  {
    pic->userData = const_cast<HWFrameMetadata*>(pMeta);
  }
  else
  {
    m_iWarnedMissing++;
    if (m_iWarnedMissing <= 3)
    {
      m_msg.log(VVENC_WARNING, "[HW] No metadata for POC %d\n", pic->poc);
    }
    pic->userData = nullptr;
  }
  return ret;
}

} // namespace vvenc
