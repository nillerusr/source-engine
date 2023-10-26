#ifndef _DEFINED_C_ASW_FIREWALL_PIECE_H
#define _DEFINED_C_ASW_FIREWALL_PIECE_H

class CASWGenericEmitter;

class C_ASW_Firewall_Piece : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Firewall_Piece, C_BaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Firewall_Piece();
	virtual			~C_ASW_Firewall_Piece();

	virtual void ClientThink();
	void OnDataChanged( DataUpdateType_t updateType );
	void CreateFireEmitter();
	CSmartPtr<CASWGenericEmitter> m_hFireEmitter;
	
private:
	C_ASW_Firewall_Piece( const C_ASW_Firewall_Piece & ); // not defined, not accessible
};

#endif /* _DEFINED_C_ASW_FIREWALL_PIECE_H */