// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/BaseToolkit.h"

class FTetherModeToolkit : public FModeToolkit
{
public:
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

private:
};
