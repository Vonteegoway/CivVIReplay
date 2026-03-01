#include "DistrictSimulator.h"
#include "imgui.h"
#include "Viewer.h"


void DistrictSimulator::Enable()
{
	CivVIState& state = loader.States[v.iCurrentState];
	/*ENABLE keep previous states ? Will need a map of what is really used in districts
	for (int i = 0; i < state.players[iPlayer].districts.size(); i++)
	{
		District& d = state.players[iPlayer].districts[i];
		state.tiles[d.x + d.y * state.width].pDistrict = NULL;
	}
	for (int i = 0; i < districts.size(); i++)
	{
		District& d = districts[i];
		state.tiles[d.x + d.y * state.width].pDistrict = &d;
	}*/
}

void DistrictSimulator::Disable()
{
	CivVIState& state = loader.States[v.iCurrentState];
	for (int i = 0; i < districts.size(); i++)
	{
		District& d = districts[i];
		state.tiles[d.x + d.y * state.width].pDistrict = NULL;
	}
	for (int i = 0; i < state.players[iPlayer].districts.size(); i++)
	{
		District& d = state.players[iPlayer].districts[i];
		state.tiles[d.x + d.y * state.width].pDistrict = &d;
	}

	districts.clear();
}

void DistrictSimulator::RemoveAll(bool keepCity)
{
	CivVIState& state = loader.States[v.iCurrentState];
	for (int i = 0; i < districts.size(); i++)
	{
		District& d = districts[i];
		if (keepCity && d.type == 0x8a32007d)
			continue;
		state.tiles[d.x + d.y * state.width].pDistrict = NULL;

	}
	for (int i = 0; i < state.players[iPlayer].districts.size(); i++)
	{
		District& d = state.players[iPlayer].districts[i];
		if (keepCity && d.type == 0x8a32007d)
			continue;
		state.tiles[d.x + d.y * state.width].pDistrict = NULL;
	}

	if(!keepCity)
		districts.clear(); //TODO for keep city
}

void DistrictSimulator::Draw()
{
	if (!enabled)
		return;

	CivVIState& State = loader.States[v.iCurrentState];

	if (ImGui::Begin("District adjacency simulator"))
	{
		char combo_label[256];
		{
			const PlayerInfo& pInfo = State.pGame->playersInfo[iPlayer];
			sprintf(combo_label, "%s (%s)", pInfo.leaderStr, pInfo.pseudo);
		}
		if (ImGui::BeginCombo("Playing as ", combo_label))
		{
			for (int i = 0; i < 64; i++)
			{
				PlayerInfo& playerInfo = State.pGame->playersInfo[i];
				if (playerInfo.typeCiv != FULL_CIV)
					continue;
				if (playerInfo.typeCiv == CITY_STATE)
					break;

				const bool is_selected = (i == iPlayer);
				char selectable_label[256];
				const PlayerInfo& pInfo = State.pGame->playersInfo[i];
				sprintf(selectable_label, "%s (%s)", pInfo.leaderStr, pInfo.pseudo);
				if (ImGui::Selectable(selectable_label, is_selected))
				{
					if (iPlayer != i)
					{
						Disable();
						iPlayer = i;
						districts = State.players[iPlayer].districts;
					}
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();

		const PlayerInfo& pInfo = State.pGame->playersInfo[iPlayer];
		unsigned int leader = pInfo.leader;
		v.DrawImage(leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		if (ImGui::Button("Remove all districts but keep cities"))
		{
			RemoveAll(true);
		}
		if (ImGui::Button("Remove all districts and cities"))
		{
			RemoveAll(false);
		}
		if (ImGui::Button("Come back to original"))
		{
			Disable();
			districts = State.players[iPlayer].districts;
			Enable();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		float width = ImGui::GetWindowWidth();
		int districtsType[] = { 0x8a32007d,0x71d9cd60,0x32b15d10,0x66db35b9,0xbe8c19bb,0x762430e5,0x885fdf1e,0xcbecc739,0xddbde5ca,0x9ac880cb,0xb92c62a7,0x41ded114,0x7aa7390d,0xc1ac5af9,0x98117336,0xe317d371,0x5dbcdc9e,0xe0ea131d };
		float posX = ImGui::GetCursorPosX();
		for (int iDistrict = 0; iDistrict < 18; iDistrict++)
		{
			int districtType = districtsType[iDistrict];
			if (iDistrict == iCurrentDistrict)
			{
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImVec2 pos = ImGui::GetCursorScreenPos();
				draw_list->AddRect(ImVec2(pos.x + 0.f, pos.y + 0.f), ImVec2(pos.x + 65.f, pos.y + 65.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f,1.f)));
			}
			v.DrawImage(districtType, 0.f, 0.f, 0.25f, ImVec4(0.f, 0.f, 1.f, 1.f));
			if (ImGui::IsItemClicked())
			{
				iCurrentDistrict = iDistrict;
			}
			posX = posX + ImGui::GetItemRectSize().x;
			if (posX < width - 125.f)
				ImGui::SameLine();
			else
				posX = ImGui::GetCursorPosX();
		}

		if (v.iSelectedTile != -1)
		{
			Tile& t = State.tiles[v.iSelectedTile];
			
			ImGui::Spacing();
			if (t.pDistrict)
			{
				if (ImGui::Button("Delete"))
				{
					if (t.iPlayer != 65535 && t.iPlayer != iPlayer)
					{
						Disable();
						iPlayer = t.iPlayer;
						districts = State.players[iPlayer].districts;
					}
					t.pDistrict = NULL;

					int x = t.iTile % State.width;
					int y = t.iTile / State.width;
					PlayerInfo& pInfo = State.pGame->playersInfo[iPlayer];

					District::computeNeighborProxi(x, y, State.tiles, State.width, pInfo, loader.assets);
				}

			}
			else
			{
				if (ImGui::Button("Build"))
				{
					if (t.iPlayer != 65535 && t.iPlayer != iPlayer)
					{
						Disable();
						iPlayer = t.iPlayer;
						districts = State.players[iPlayer].districts;
					}
					PlayerInfo& pInfo = State.pGame->playersInfo[iPlayer];
					districts.push_back(District());
					District& d = districts.back();
					d.type = District::specificDistrict(districtsType[iCurrentDistrict], pInfo);
					d.x = t.iTile % State.width;
					d.y = t.iTile / State.width;
					d.built = 1;
					d.cityId = 0;
					d.computeProxi(State.tiles, State.width, pInfo, loader.assets);
					State.tiles[t.iTile].pDistrict = &d;
					District::computeNeighborProxi(d.x, d.y, State.tiles, State.width, pInfo, loader.assets);
				}

			}

		}
	}

	ImGui::End();
}