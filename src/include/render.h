#pragma once

typedef struct
{
	ImU32 R;
	ImU32 G;
	ImU32 B;
} RGB;

ImU32 Color(RGB color, float alpha)
{
	return IM_COL32(color.R, color.G, color.B, alpha);
}

namespace Render
{
	void Rect(int x, int y, int w, int h, RGB color, int thickness, bool isFilled, float alpha)
	{
		if (!isFilled)
			ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), Color(color, alpha), 0, 0, thickness);
		else
			ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Color(color, alpha), 0, 0);
	}

	void Line(float x1, float y1, float x2, float y2, RGB color, int thickness, float alpha)
	{
		ImGui::GetBackgroundDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), Color(color, alpha), thickness);
	}

	void Circle(float x, float y, float radius, RGB color, bool isFilled, float alpha)
	{
		if (!isFilled)
			ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(x, y), radius, Color(color, alpha), 0, 1);
		else
			ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(x, y), radius, Color(color, alpha), 0);
	}

	void Text(int x, int y, const char *text, RGB color)
	{
		ImGui::GetBackgroundDrawList()->AddText(ImVec2(x, y), Color(color, 255), text);
	}
}