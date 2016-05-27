#ifndef FREESHOP_APPLIST_HPP
#define FREESHOP_APPLIST_HPP

#include <vector>
#include <string>
#include <TweenEngine/TweenManager.h>
#include "AppItem.hpp"
#include "RichText.hpp"


namespace FreeShop
{

class AppList : public cpp3ds::Drawable, public util3ds::TweenTransformable<cpp3ds::Transformable> {
public:
	enum SortType {
		AlphaNumericAsc,
		AlphaNumericDesc,
	};

	AppList(std::string jsonFilename);
	~AppList();
	void refresh();
	void setSortType(SortType sortType);
	SortType getSortType() const;

	void setSelectedIndex(int index);
	int getSelectedIndex() const;

	size_t getCount() const;
	size_t getVisibleCount() const;

	AppItem* getSelected();

	void setCollapsed(bool collapsed);
	bool isCollapsed() const;

	void filterBySearch(const std::string& searchTerm, std::vector<util3ds::RichText> &textMatches);

	void update(float delta);

protected:
	virtual void draw(cpp3ds::RenderTarget& target, cpp3ds::RenderStates states) const;
	void sort();
	void resize();

private:
	SortType m_sortType;
	std::string m_jsonFilename;
	int m_selectedIndex;
	std::vector<std::unique_ptr<AppItem>> m_list;
	std::vector<cpp3ds::Texture*> m_iconTextures;
	bool m_collapsed;
	TweenEngine::TweenManager m_tweenManager;
};

} // namespace FreeShop

#endif // FREESHOP_APPLIST_HPP
