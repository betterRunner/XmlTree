#ifndef XML_TREE_HPP_INCLUDED
#define XML_TREE_HPP_INCLUDED

#include <string.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <algorithm>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"



/**
 * @note  set EN_LogMsg to 1 log message, set 0 otherwise.
 */
#define EN_LogMsg                         1u

#if EN_LogMsg > 0u
  #define  __logMsg(...) printf(__VA_ARGS__)
#else
  #define  __logMsg(...) (void *)0
#endif

#define  __logVal(val) \
  do{ \
    switch(val->e_type){ \
    case VAL_String: \
      __logMsg("%s(string) ", val->u_val.val_string); \
      break; \
    case VAL_Int: \
      __logMsg("%d(int) ", val->u_val.val_int); \
      break; \
    case VAL_Double: \
      __logMsg("%f(double) ", val->u_val.val_double); \
      break; \
    default: \
      break; \
    } \
  }while(0)



/**
 * @brief This namespace gives out a multi-tree data structure used to
 *        store the data whose members have parent-child relationship
 *        like multi-tree.
 *
 *        With this tree, if you have multi data with tree-structure,
 *        you don't have to store the value in form of tree in xml file,
 *        instead, you can use one xml file (xml_name.xml) to build the
 *        structrue of tree and give name to each item, and use another
 *        file (xml_val.xml) to store batches of value in parallel.
 *
 * @note  (1) The class 'xmlTree' contains all functions to build the
 *            tree and access the data.
 *
 *        (2) Data of tree is using struct "xmlItem_Val_t", which is only
 *            with some specific types, user should use this struct to
 *            access data.
 *
 *        (3) Items' name are from "xml_name.xml" and items' value are
 *            from "xml_val.xml".
 *
 *        (4) All operation about xml file is using "rapidxml".
 */
namespace xml_tree
{
  /**
   * @note enum of error code.
   */
  enum Tree_Error_e
  {
    ERR_None = 0,
    ERR_OverLayer,
    ERR_NullPointer,
    ERR_OverItem,
    ERR_NoXmlNode,
    ERR_NoXmlAttr,
    ERR_IllegalIndex,
    ERR_IllegalId,
    ERR_UsedIndex,
    ERR_UnregisteredIndex,
    ERR_UnregisteredItem,
  };

  /**
   * @note  VAL_None means no value.
   */
  enum Tree_Val_e
  {
    VAL_None = 0,
    VAL_Int,
    VAL_String,
    VAL_Double,

    VAL_NUM,
  };

  /**
   * @note We only support the format below.
   */
  union Tree_Val_u
  {
    int val_int;
    double val_double;
    char* val_string;
  };

  /**
   * @brief Struct to store value, where e_type shands for type of value,
   *        u_val stands for the union space to store value, n_memLen
   *        stands for the length of space only if type of value need to
   *        allocate.
   *
   * @note  user should use this struct to read and write value.
   */
  struct Tree_Val_t
  {
    Tree_Val_e e_type;                // Enum of type of value.
    Tree_Val_u u_val;                 // Union of value.
    int n_memLen;                          // Len of memory of value.

    /* copy from another struct. */
    Tree_Val_t& operator =(const Tree_Val_t &val)
    {
      this->e_type = val.e_type;
      switch(this->e_type)
      {
      case VAL_String:
        this->n_memLen = val.n_memLen;
        this->u_val.val_string = new char[val.n_memLen];
        memcpy(this->u_val.val_string, val.u_val.val_string, val.n_memLen);
        break;
      case VAL_Int:
        this->u_val.val_int = val.u_val.val_int;
        break;
      case VAL_Double:
        this->u_val.val_double = val.u_val.val_double;
        break;
      default:
        break;
      }
      return (*this);
    }

    /* compare with another struct. */
    bool operator ==(const Tree_Val_t &val) const
    {
      if(this->e_type == val.e_type)
      {
        switch(this->e_type)
        {
        case VAL_String:
          return !memcmp(this->u_val.val_string, val.u_val.val_string, val.n_memLen);
          break;
        case VAL_Int:
          return this->u_val.val_int == val.u_val.val_int;
          break;
        case VAL_Double:
          return this->u_val.val_double == val.u_val.val_double;
          break;
        default:
          break;
        }
      }
      else
      {
        return 0;
      }
    }
  };

  /**
   * @brief A class to build and access the xml tree.
   *
   * @note  (1) How to use this class:
   *            1. Use build_tree_fromXmlFile() to build the tree from
   *               "xml_name.xml".
   *            2. Use add_batch_fromXmlFile() to set batches of value
   *               to items in tree from "xml_val.xml".
   *            3. Then a tree with user value is built, use other function
   *               to operate the user value.
   *
   *        (2) Since the tree item's id is type uint32_t, where one layer
   *            needs 4 bit to set the index from 1~15, so the max number of
   *            child items of each item is 15, the max number of layer of
   *            tree is 8.
   *
   *        (3) Also look "xml_name.xml" and "xml_val.xml" to know more
   *            well about how to use.
   */
  class xmlTree
  {
  public:
#define FORMAT_Item_Id         16                 // item id use hex format.
#define FORMAT_Batch_Index     10                 // batch index use 10 format.

    xmlTree()
    {
      s_rootItem.n_id = 0;
    }

    ~xmlTree()
    {
      _free_itemTree(&s_rootItem);
      __logMsg("xml tree free succ\r\n.");
    }

    /**
     * @brief This func build the tree by "xml_name.xml",which contains
     *        the name of item and structure of tree.
     *
     * @input str_xml_name: name of xml file.
	 *
	 * @ret   return ERR_None if success otherwise return error code.
     */
    int build_tree_fromXmlFile(const char* str_xml_name)
    {
      int ret = ERR_None;

      rapidxml::file<> xml_file(str_xml_name);
      rapidxml::xml_document<> xml_doc;
      xml_doc.parse<0>(xml_file.data());

      rapidxml::xml_node<>* root_node = xml_doc.first_node();
      ret = _make_itemTree(root_node, &s_rootItem, 0);
      if(ret == ERR_None)
      {
        __logMsg("\r\n xml tree build succ.\r\n");
      }
      else
      {
        __logMsg("\r\n xml tree build fail, err code: %d\r\n", ret);
      }

      return ret;
    }

    /**
     * @brief This func set batches of value of item by an xml file,
     *        which stores batches of value in parallel mode.
     *
     * @input str_xml_val: name of value xml file.
	 *
	 * @ret   return ERR_None if success otherwise return error code.
     */
    int add_batch_fromXmlFile(const char* str_xml_val)
    {
      int ret = ERR_None;

      rapidxml::file<> xml_file(str_xml_val);
      rapidxml::xml_document<> xml_doc;
      xml_doc.parse<0>(xml_file.data());

      rapidxml::xml_node<>* root_node = xml_doc.first_node();
      /* use a loop to get all bathes of value in xml file. */
      rapidxml::xml_node<>* batch_node = root_node->first_node(C_strBatchTag.c_str());
      for( ; batch_node != NULL; batch_node = batch_node->next_sibling(C_strBatchTag.c_str()))
      {
        uint32_t batch_index = _get_batchIndex(batch_node);
        __logMsg("\r\nadding batch %d\r\n", batch_index);
        if(ret = _push_memberVector(batch_node->first_node(), batch_index) != ERR_None)
        {
          break; // exit the loop if operation is illegal.
        }
        set_batchIndex.insert(batch_index); // insert to batch index set for record usage.
      }
      if(ret == 0)
      {
        __logMsg("\r\n xml tree set value succ.\r\n");
      }
      else
      {
        __logMsg("\r\n xml tree set value failed, err code: %d\r\n", ret);
      }

      return ret;
    }

    /**
     * @brief This func get the name of one item.
     *
     * @input n_itemId: id of item, look "xml_name.xml" for its combination.
	 *
	 * @ret   return name of item.
     */
    const char* get_itemName(uint32_t n_itemId)
    {
      Tree_Item_t *item = _search_item_byId(n_itemId);
      if(item != NULL)
      {
        if(!item->str_name.empty())
        {
          return item->str_name.c_str();
        }
      }
      return NULL;
    }

    /**
     * @brief This func return the set of batch index.
     *
     * @note  (1) User can check this set with the batch index
     *            in "xml_val.xml", they should be the same.
     */
    void get_batchSet(std::set<uint32_t> &set_batch) const
    {
      set_batch.clear(); // clear the original element first.
      for(auto iter = set_batchIndex.rbegin(); iter != set_batchIndex.rend(); ++iter)
      {
        set_batch.insert((*iter)); // copy from set_batchIndex.
      }
    }

    /**
     * @brief This func get one batch of value by its index.
     *
     * @input n_batchIndex: index of batch.
     * @output m_batch: map of pair <name, val>.
	 *
	 * @ret   return ERR_None if suceess otherwise return error code.
     *
     * @note  (1) the val element in m_batch is allocate in this function,
     *            so remember to free space of it before clear the map.
     *            The way to free:
     *            {
     *              if(val->e_type == VAL_String) delete val->u_val.val_string;
     *              delete val;
     *            }
     */
    int get_oneBatchValue(uint32_t n_batchIndex, std::map<std::string, Tree_Val_t*> &m_batch) const
    {
      auto iter = set_batchIndex.find(n_batchIndex);
      if(iter != set_batchIndex.end())
      {
        _get_membersOfBatch(&s_rootItem, n_batchIndex, m_batch);
        return ERR_None;
      }
      return ERR_UnregisteredIndex;
    }

    /**
     * @brief This func get value of once item by its item id.
     *
     * @input n_itemId: id of item.
     * @output m_item: map of pair <batch_index, val>.
     *
     * @ret return return ERR_None if success otherwise return error code.
     *
     * @note  (1) the val element in m_batch is allocate in this function,
     *            so remember to free space of it before clear the map.
     *            The way to free:
     *            {
     *              if(val->e_type == VAL_String) delete val->u_val.val_string;
     *              delete val;
     *            }
     */
    int get_oneItemValue(const char* str_itemName, std::map<uint32_t, Tree_Val_t*> &m_item)
    {
      Tree_Item_t *item = _search_item_byName(&s_rootItem, str_itemName);
      if(item != NULL)
      {
        _get_membersOfItem(item, m_item);
        return ERR_None;
      }
      return ERR_UnregisteredItem;
    }

    /**
     * @brief This func delete one batch of value.
     *
     * @input n_batchIndex: index of batch.
     *
     * @ret   Return ERR_None if the n_batchIndex is legal,
	 *        return error code otherwise.
     */
     int delete_oneBatch(uint32_t n_batchIndex)
     {
        auto iter = set_batchIndex.find(n_batchIndex);
        if(iter != set_batchIndex.end())
        {
          _delete_membersOfBatch(&s_rootItem, n_batchIndex);
          set_batchIndex.erase(iter);
          return 0;
        }
        return ERR_UnregisteredIndex;
     }

  private:

    /**
     * @note no copying!
     */
    xmlTree(const xmlTree &);
    void operator =(const xmlTree &);

    struct Tree_Member_t
    {
      Tree_Val_t s_val;                               // Struct of value of member.
      std::set<uint32_t> set_batchIndex;              // Set of index of batch that this member is in.
    };

    struct Tree_Item_t
    {
      uint32_t n_id;                                  // id of item.
      std::string str_name;                           // Name of item.

      std::list<Tree_Member_t *> l_member;              // List of member of item.
      std::vector<Tree_Item_t *> v_childItem;           // Vector of all childs' item.
    };

    /**
     * @ret Return ERR_None if build item tree succeed, otherwise return error code.
     *      Conditions to succeed:
     *      (1) all sub layers return 0. (ret |= _make_itemTree())
     *      (2) all item in this layer is legal.
     *          ((cnt_item != 0) && (cnt_item == cnt_legalItem))
     *      Or
     *      (1) in last layer directly return 0.
     *          ((node_parent->first_node() == NULL) && (n_layer > 0))
     */
    int _make_itemTree(const rapidxml::xml_node<>* node_parent, Tree_Item_t *item_parent, int n_layer)
    {
      int ret = ERR_NullPointer;

      if((node_parent != NULL) && (item_parent != NULL)) // node and item is not null.
      {
        ret = ERR_OverLayer;
        if(n_layer < C_nMaxLayer) // the layer number can not >= C_nMaxLayer.
        {
          if((node_parent->first_node() == NULL) && (n_layer > 0)) return ERR_None; // the last layer directly return ERR_None.

          ret = ERR_NoXmlNode;
          int last_err = ERR_None;
          int cnt_item = 0, cnt_legalItem = 0;
          std::set<uint32_t> index_set;
          rapidxml::xml_node<>* child_node = node_parent->first_node(C_strItemTag.c_str());
          for( ; child_node != NULL; cnt_item++, child_node = child_node->next_sibling(C_strItemTag.c_str())) // should have item node.
          {
            if(cnt_item > C_nMaxItem) //should contain fewer than C_nMaxItem items.
            {
              ret = ERR_OverItem;
              break;
            }
            ret = ERR_NoXmlAttr;
            char *p_char;
            Tree_Item_t *child_item = new Tree_Item_t;
            rapidxml::xml_attribute<>* temp_attr = child_node->first_attribute(C_strIndexTag.c_str());
            if(temp_attr != NULL) // should have item layer index attribute.
            {
              ret = ERR_IllegalIndex;
              uint32_t item_index = static_cast<uint32_t>(strtol(temp_attr->value(), &p_char, 10));
              if((item_index != 0) && (item_index <= C_nMaxItem) && (index_set.find(item_index) == index_set.end())) // item index is legal and not used.
              {
                child_item->n_id = (item_index << C_nCrorNum * n_layer) | item_parent->n_id;
                child_item->str_name = child_node->first_attribute(C_strNameTag.c_str())->value();
                item_parent->v_childItem.push_back(child_item); // put the item into child vector.
                __logMsg("add item: name(%s) id(%08x)\r\n",child_item->str_name.c_str(), child_item->n_id);

                index_set.insert(item_index);
                cnt_legalItem ++;
                last_err |= _make_itemTree(child_node, child_item, n_layer+1);
              }
            }
          }
          index_set.clear();
          if((cnt_item != 0) && (cnt_item == cnt_legalItem))
          {
            ret = last_err;
          }
          else
          {
            ret = ret;
          }
        }
      }
      return ret;
    }

    void _free_itemTree(Tree_Item_t *item_cur)
    {
      if(item_cur != NULL)
      {
        /* free the space of value. */
        for(auto iter = item_cur->l_member.begin(); iter != item_cur->l_member.end(); ++iter)
        {
          Tree_Member_t *member = (*iter);
          if(member->s_val.e_type == VAL_String)
          {
            delete[] member->s_val.u_val.val_string;
          }
          delete member;
        }
        item_cur->l_member.clear();

        for(auto iter = item_cur->v_childItem.begin(); iter != item_cur->v_childItem.end(); ++iter)
        {
          Tree_Item_t *temp_item = (*iter);
          _free_itemTree(temp_item);
        }

        item_cur->v_childItem.clear();
        /* free the space of item. */
        delete item_cur;
      }
    }

    Tree_Item_t *_search_item_byId(uint32_t n_id)
    {
      Tree_Item_t *temp_item = &s_rootItem;
      uint32_t temp_id = n_id;

      if(n_id == 0) return &s_rootItem;

      while((temp_item != NULL) && (temp_id != 0))
      {
        uint32_t index = (temp_id & C_nMaxItem);
        if((index == 0) || (index > temp_item->v_childItem.size()))
        {
          break;
        }
        temp_item = temp_item->v_childItem[index-1];
        if(temp_item->n_id == n_id)
        {
          return temp_item;
        }
        temp_id >>= C_nCrorNum;
      }
      return NULL;
    }

    Tree_Item_t* _search_item_byName(const Tree_Item_t *item_parent, const char* str_itemName) const
    {
      uint32_t id = 0;
      Tree_Item_t* ret_item = NULL;

      if(item_parent != NULL)
      {
        for(auto iter = item_parent->v_childItem.begin(); iter != item_parent->v_childItem.end(); ++iter)
        {
          Tree_Item_t *child_item = (*iter);
          ret_item = _search_item_byName(child_item, str_itemName);
          if(!strcmp(child_item->str_name.c_str(), str_itemName))
          {
            ret_item = child_item;
          }
          if(ret_item != NULL) break;
        }
      }
      return ret_item;
    }

    uint32_t _get_parentId(uint32_t id_child) const
    {
      if(id_child != 0)
      {
        int cror = 0;
        while(id_child)
        {
          cror++;
          id_child >>= C_nCrorNum;
        }
        return id_child & ((1 << C_nCrorNum*(cror-1)) - 1);
      }
      return 0;
    }

    /**
     * @ret Return ERR_None if push member in vector succeed otherwise return error code.
     *      Condition to succeed:
     *      (1) all members in this batch are pushed successfully.
     *          (ret |= _push_memberVector())
     */
    int _push_memberVector(const rapidxml::xml_node<>* node_member, uint32_t index_batch)
    {
      int ret = ERR_None;

      if(node_member != NULL) // node is not null.
      {
        ret = ERR_IllegalIndex;
        if(index_batch > 0) // index must > 0.
        {
          ret = ERR_NoXmlAttr;
          rapidxml::xml_attribute<>* temp_attr;
          if((temp_attr = node_member->first_attribute(C_strNameTag.c_str())) != NULL) // node has name attribute.
          {
            ret = ERR_IllegalId;
            Tree_Item_t *member_item = _search_item_byName(&s_rootItem, temp_attr->value());
            if((member_item != NULL) && (_search_item_byId(_get_parentId(member_item->n_id)) != NULL)) // the id of item must be legal.
            {
              ret = ERR_UsedIndex;
              Tree_Val_t temp_val;
              if(_get_memberVal(member_item, index_batch, temp_val) == ERR_UnregisteredIndex) // this batch index has not been used.
              {
                ret = ERR_NoXmlAttr;
                if((temp_attr = node_member->first_attribute(C_strTypeTag.c_str())) != NULL) // node has type attribute.
                {
                  ret = ERR_None;
                  Tree_Val_t temp_val;
                  temp_val.e_type = _parse_strToType(temp_attr->value());
                  _set_memberVal(node_member->value(), temp_val);

                  __logMsg("item (%s) add value: ",member_item->str_name.c_str());__logVal((&temp_val));__logMsg("\r\n");

                  auto iter = std::find_if(member_item->l_member.begin(), member_item->l_member.end(),
                  [=](Tree_Member_t *member){
                    return member->s_val == temp_val;
                  });
                  if(iter == member_item->l_member.end()) // the member with this val is not in vector, push a new one.
                  {
                    Tree_Member_t *new_member = new Tree_Member_t;
                    new_member->s_val = temp_val;
                    new_member->set_batchIndex.insert(index_batch);
                    member_item->l_member.push_back(new_member);
                  }
                  else // the member with this val is already in vector, only push the batch id to member's id vector (save space).
                  {
                    Tree_Member_t *member = (*iter);
                    member->set_batchIndex.insert(index_batch);
                  }
                  return _push_memberVector(node_member->next_sibling(), index_batch);
                }
              }
            }
          }
        }
      }
      return ret;
    }

    void _set_memberVal(const std::string &str_val, Tree_Val_t &s_val)
    {
      switch(s_val.e_type)
      {
      case VAL_String:
        {
          s_val.n_memLen = str_val.length()+1;
          char* val = new char[s_val.n_memLen];
          memcpy(val, str_val.c_str(), s_val.n_memLen);
          s_val.u_val.val_string = val;
          break;
        }
      case VAL_Int:
        {
          char *p_char;
          int val = strtol(str_val.c_str(), &p_char, 10);
          s_val.u_val.val_int = val;
        }
        break;
      case VAL_Double:
        {
          char *p_char;
          double val = strtod(str_val.c_str(), &p_char);
          s_val.u_val.val_double = val;
        }
        break;
      default:
        break;
      }
    }

    int _get_memberVal(const Tree_Item_t *item_member, uint32_t n_batchIndex, Tree_Val_t &s_val) const
    {
      if(item_member != NULL)
      {
        for(auto iter = item_member->l_member.begin(); iter != item_member->l_member.end(); ++iter)
        {
          Tree_Member_t* member = (*iter);
          auto iter2 = member->set_batchIndex.find(n_batchIndex);
          if(iter2 != member->set_batchIndex.end())
          {
            Tree_Member_t *member = (*iter);
            s_val = member->s_val;
            return ERR_None; // get the val.
          }
        }
      }
      return ERR_UnregisteredIndex; // not get the val.
    }

    uint32_t _get_batchIndex(rapidxml::xml_node<>* node_batch)
    {
      rapidxml::xml_attribute<>* temp_attr;
      if((temp_attr = node_batch->first_attribute(C_strIndexTag.c_str())) != NULL) // node has index attribute.
      {
        char *p_char;
        uint32_t batch_index = strtol(temp_attr->value(), &p_char, FORMAT_Batch_Index);
        return batch_index;
      }
      return 0;
    }

    void _get_membersOfBatch(const Tree_Item_t* item_cur, uint32_t n_batchIndex, std::map<std::string, Tree_Val_t*> &m_batch) const
    {
      if(item_cur != NULL)
      {
        Tree_Val_t *new_val = new Tree_Val_t;
        new_val->e_type = VAL_None;
        _get_memberVal(item_cur, n_batchIndex, *new_val); // get the value from tree.
        m_batch.insert(std::make_pair(item_cur->str_name, new_val)); // insert name and value into batch map.

        for(auto iter=item_cur->v_childItem.begin(); iter!=item_cur->v_childItem.end(); ++iter)
        {
          Tree_Item_t *child_item = (*iter);
          _get_membersOfBatch(child_item, n_batchIndex, m_batch);
        }
      }
    }

    void _get_membersOfItem(const Tree_Item_t* item_cur, std::map<uint32_t, Tree_Val_t*> &m_item) const
    {
      for(auto iter=item_cur->l_member.begin(); iter!=item_cur->l_member.end(); ++iter)
      {
        Tree_Member_t *member = (*iter);
        for(auto iter2=member->set_batchIndex.begin(); iter2!=member->set_batchIndex.end(); ++iter2)
        {
          uint32_t batch_index = (*iter2);
          Tree_Val_t* new_val = new Tree_Val_t;
          new_val->e_type = VAL_None;
          _get_memberVal(item_cur, batch_index, *new_val); // get the value from tree.
          m_item.insert(std::make_pair(batch_index, new_val)); // insert batch index and value into item map.
        }
      }
    }

    void _delete_membersOfBatch(Tree_Item_t* item_cur, uint32_t n_bathIndex)
    {
      bool is_delete = false;
      if(item_cur != NULL)
      {
        for(auto iter=item_cur->l_member.begin(); iter!=item_cur->l_member.end(); )
        {
          Tree_Member_t *member = (*iter);
          auto it = member->set_batchIndex.find(n_bathIndex);
          if(it != member->set_batchIndex.end())
          {
            member->set_batchIndex.erase(it);
            if(member->set_batchIndex.size() == 0) // this member is only owned by this index, delete the member also.
            {
              if(member->s_val.e_type == VAL_String)
              {
                delete[] member->s_val.u_val.val_string;
              }
              delete member;
              is_delete = true;
            }
          }
          if(is_delete)
          {
            iter = item_cur->l_member.erase(iter);
          }
          else
          {
            iter++;
          }
        }

        for(auto iter=item_cur->v_childItem.begin(); iter!=item_cur->v_childItem.end(); ++iter)
        {
          Tree_Item_t *child_item = (*iter);
          _delete_membersOfBatch(child_item, n_bathIndex);
        }
      }
    }

    Tree_Val_e _parse_strToType(const char* str_type) const
    {
      int m;
      for(m=0; m<VAL_NUM; m++)
      {
        if(strcmp(C_arrValTypeStr[m].c_str(), str_type) == 0) break;
      }
      return static_cast<Tree_Val_e>(m);
    };

    const static int C_nMaxLayer;
    const static int C_nMaxItem;
    const static int C_nCrorNum;
    const static std::string C_strItemTag;
    const static std::string C_strIndexTag;
    const static std::string C_strNameTag;
    const static std::string C_strBatchTag;
    const static std::string C_strMemberTag;
    const static std::string C_strTypeTag;
    const static std::string C_arrValTypeStr[VAL_NUM];

    Tree_Item_t s_rootItem; // Root item of xmlTree.
    std::set<uint32_t> set_batchIndex; // Vector of index of all batches.
  };

  const int xmlTree::C_nMaxLayer = sizeof(uint32_t) * 2; // 0x00 has 2 'bit' to use, 0x0000 has 4 'bit', and so on.
  const int xmlTree::C_nMaxItem = FORMAT_Item_Id - 1; // 0xf, each 'bit' max is 15.
  const int xmlTree::C_nCrorNum = 4; // 0x1 -> 0x10 need cror 4 bits.
  const std::string xmlTree::C_strItemTag = "Content";
  const std::string xmlTree::C_strBatchTag = "Batch";
  const std::string xmlTree::C_strMemberTag = "Member";
  const std::string xmlTree::C_strIndexTag = "index";
  const std::string xmlTree::C_strNameTag = "name";
  const std::string xmlTree::C_strTypeTag = "type";
  const std::string xmlTree::C_arrValTypeStr[] = {"", "int", "string", "double"};
}

namespace xml_tree
{
  /**
   * @brief this func gave a basic demo of how to use xml_tooxbox.
   */
  void demo()
  {
    xml_tree::xmlTree xml_tree;

    if(xml_tree.build_tree_fromXmlFile("xml_name.xml") == 0) // build tree success.
    {
      if(xml_tree.add_batch_fromXmlFile("xml_val.xml") == 0) // set value success.
      {
        /* 1.get the set that store batch index. */
        __logMsg("\r\n 1.batch set info :\r\n\r\n");
        std::set<uint32_t> index_set;
        xml_tree.get_batchSet(index_set);
        __logMsg("batch num :%d\r\n", index_set.size());
        for(auto iter=index_set.rbegin(); iter!=index_set.rend(); ++iter)
        {
          uint32_t index = (*iter);
          __logMsg("batch index :%d\r\n", index);
        }

        /* 2.get value in one item. */
        __logMsg("\r\n 2.item student has value :\r\n\r\n");
        std::map<uint32_t, Tree_Val_t*> item_map;
        xml_tree.get_oneItemValue("student", item_map);
        for(auto iter = item_map.begin(); iter!=item_map.end(); ++iter)
        {
          uint32_t batch_index = iter->first;
          Tree_Val_t *val = iter->second;
          __logMsg("%d: ", batch_index);__logVal(val);__logMsg("\r\n");
        }

        /* 3.get one batch of user data. */
        __logMsg("\r\n 3.batch 2 content :\r\n\r\n");
        std::map<std::string, Tree_Val_t*> batch_map;
        xml_tree.get_oneBatchValue(2, batch_map);
        for(auto iter = batch_map.begin(); iter!=batch_map.end(); ++iter)
        {
          std::string name = iter->first;
          Tree_Val_t *val = iter->second;
          __logMsg("%s: ", name.c_str());__logVal(val);__logMsg("\r\n");
        }

        /* 4.delete one batch */
        xml_tree.delete_oneBatch(2);
        xml_tree.get_batchSet(index_set);
        __logMsg("\r\n 4.batch num :%d\r\n", index_set.size());
      }
    }
  }
}
#endif
