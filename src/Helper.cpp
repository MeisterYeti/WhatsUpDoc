#include "Helper.h"

namespace WhatsUpDoc {

std::ostream& operator<<(std::ostream& stream, const CXString& str) {
  auto cstr = clang_getCString(str);
  if(cstr && cstr[0] != '\0')
    stream << cstr;
  clang_disposeString(str);
  return stream;
}

std::string toString(const CXString& str) {
  auto cstr = clang_getCString(str);
  std::string s;
  if(cstr && cstr[0] != '\0')
    s = cstr;
  clang_disposeString(str);
  return s;
}

bool isLiteral(CXCursorKind kind) {  
  switch(kind) {
    case CXCursor_IntegerLiteral:
    case CXCursor_FloatingLiteral:
    case CXCursor_ImaginaryLiteral:
    case CXCursor_StringLiteral:
    case CXCursor_CharacterLiteral:
      return true;
    default:
      return false;
  }
}

std::string getCursorKindName(CXCursorKind kind) {
  switch(kind) {
    case CXCursor_UnexposedDecl: return "UnexposedDecl"; // 1
    case CXCursor_StructDecl: return "StructDecl"; // 2
    case CXCursor_UnionDecl: return "UnionDecl"; // 3
    case CXCursor_ClassDecl: return "ClassDecl"; // 4
    case CXCursor_EnumDecl: return "EnumDecl"; // 5
    case CXCursor_FieldDecl: return "FieldDecl"; // 6
    case CXCursor_EnumConstantDecl: return "EnumConstantDecl"; // 7
    case CXCursor_FunctionDecl: return "FunctionDecl"; // 8
    case CXCursor_VarDecl: return "VarDecl"; // 9
    case CXCursor_ParmDecl: return "ParmDecl"; // 10
    case CXCursor_ObjCInterfaceDecl: return "ObjCInterfaceDecl"; // 11
    case CXCursor_ObjCCategoryDecl: return "ObjCCategoryDecl"; // 12
    case CXCursor_ObjCProtocolDecl: return "ObjCProtocolDecl"; // 13
    case CXCursor_ObjCPropertyDecl: return "ObjCPropertyDecl"; // 14
    case CXCursor_ObjCIvarDecl: return "ObjCIvarDecl"; // 15
    case CXCursor_ObjCInstanceMethodDecl: return "ObjCInstanceMethodDecl"; // 16
    case CXCursor_ObjCClassMethodDecl: return "ObjCClassMethodDecl"; // 17
    case CXCursor_ObjCImplementationDecl: return "ObjCImplementationDecl"; // 18
    case CXCursor_ObjCCategoryImplDecl: return "ObjCCategoryImplDecl"; // 19
    case CXCursor_TypedefDecl: return "TypedefDecl"; // 20
    case CXCursor_CXXMethod: return "CXXMethod"; // 21
    case CXCursor_Namespace: return "Namespace"; // 22
    case CXCursor_LinkageSpec: return "LinkageSpec"; // 23
    case CXCursor_Constructor: return "Constructor"; // 24
    case CXCursor_Destructor: return "Destructor"; // 25
    case CXCursor_ConversionFunction: return "ConversionFunction"; // 26
    case CXCursor_TemplateTypeParameter: return "TemplateTypeParameter"; // 27
    case CXCursor_NonTypeTemplateParameter: return "NonTypeTemplateParameter"; // 28
    case CXCursor_TemplateTemplateParameter: return "TemplateTemplateParameter"; // 29
    case CXCursor_FunctionTemplate: return "FunctionTemplate"; // 30
    case CXCursor_ClassTemplate: return "ClassTemplate"; // 31
    case CXCursor_ClassTemplatePartialSpecialization: return "ClassTemplatePartialSpecialization"; // 32
    case CXCursor_NamespaceAlias: return "NamespaceAlias"; // 33
    case CXCursor_UsingDirective: return "UsingDirective"; // 34
    case CXCursor_UsingDeclaration: return "UsingDeclaration"; // 35
    case CXCursor_TypeAliasDecl: return "TypeAliasDecl"; // 36
    case CXCursor_ObjCSynthesizeDecl: return "ObjCSynthesizeDecl"; // 37
    case CXCursor_ObjCDynamicDecl: return "ObjCDynamicDecl"; // 38
    case CXCursor_CXXAccessSpecifier: return "CXXAccessSpecifier"; // 39
    case CXCursor_ObjCSuperClassRef: return "ObjCSuperClassRef"; // 40
    case CXCursor_ObjCProtocolRef: return "ObjCProtocolRef"; // 41
    case CXCursor_ObjCClassRef: return "ObjCClassRef"; // 42
    case CXCursor_TypeRef: return "TypeRef"; // 43
    case CXCursor_CXXBaseSpecifier: return "CXXBaseSpecifier"; // 44
    case CXCursor_TemplateRef: return "TemplateRef"; // 45
    case CXCursor_NamespaceRef: return "NamespaceRef"; // 46
    case CXCursor_MemberRef: return "MemberRef"; // 47
    case CXCursor_LabelRef: return "LabelRef"; // 48
    case CXCursor_OverloadedDeclRef: return "OverloadedDeclRef"; // 49
    case CXCursor_VariableRef: return "VariableRef"; // 50
    case CXCursor_InvalidFile: return "InvalidFile"; // 70
    case CXCursor_NoDeclFound: return "NoDeclFound"; // 71
    case CXCursor_NotImplemented: return "NotImplemented"; // 72
    case CXCursor_InvalidCode: return "InvalidCode"; // 73
    case CXCursor_UnexposedExpr: return "UnexposedExpr"; // 100
    case CXCursor_DeclRefExpr: return "DeclRefExpr"; // 101
    case CXCursor_MemberRefExpr: return "MemberRefExpr"; // 102
    case CXCursor_CallExpr: return "CallExpr"; // 103
    case CXCursor_ObjCMessageExpr: return "ObjCMessageExpr"; // 104
    case CXCursor_BlockExpr: return "BlockExpr"; // 105
    case CXCursor_IntegerLiteral: return "IntegerLiteral"; // 106
    case CXCursor_FloatingLiteral: return "FloatingLiteral"; // 107
    case CXCursor_ImaginaryLiteral: return "ImaginaryLiteral"; // 108
    case CXCursor_StringLiteral: return "StringLiteral"; // 109
    case CXCursor_CharacterLiteral: return "CharacterLiteral"; // 110
    case CXCursor_ParenExpr: return "ParenExpr"; // 111
    case CXCursor_UnaryOperator: return "UnaryOperator"; // 112
    case CXCursor_ArraySubscriptExpr: return "ArraySubscriptExpr"; // 113
    case CXCursor_BinaryOperator: return "BinaryOperator"; // 114
    case CXCursor_CompoundAssignOperator: return "CompoundAssignOperator"; // 115
    case CXCursor_ConditionalOperator: return "ConditionalOperator"; // 116
    case CXCursor_CStyleCastExpr: return "CStyleCastExpr"; // 117
    case CXCursor_CompoundLiteralExpr: return "CompoundLiteralExpr"; // 118
    case CXCursor_InitListExpr: return "InitListExpr"; // 119
    case CXCursor_AddrLabelExpr: return "AddrLabelExpr"; // 120
    case CXCursor_StmtExpr: return "StmtExpr"; // 121
    case CXCursor_GenericSelectionExpr: return "GenericSelectionExpr"; // 122
    case CXCursor_GNUNullExpr: return "GNUNullExpr"; // 123
    case CXCursor_CXXStaticCastExpr: return "CXXStaticCastExpr"; // 124
    case CXCursor_CXXDynamicCastExpr: return "CXXDynamicCastExpr"; // 125
    case CXCursor_CXXReinterpretCastExpr: return "CXXReinterpretCastExpr"; // 126
    case CXCursor_CXXConstCastExpr: return "CXXConstCastExpr"; // 127
    case CXCursor_CXXFunctionalCastExpr: return "CXXFunctionalCastExpr"; // 128
    case CXCursor_CXXTypeidExpr: return "CXXTypeidExpr"; // 129
    case CXCursor_CXXBoolLiteralExpr: return "CXXBoolLiteralExpr"; // 130
    case CXCursor_CXXNullPtrLiteralExpr: return "CXXNullPtrLiteralExpr"; // 131
    case CXCursor_CXXThisExpr: return "CXXThisExpr"; // 132
    case CXCursor_CXXThrowExpr: return "CXXThrowExpr"; // 133
    case CXCursor_CXXNewExpr: return "CXXNewExpr"; // 134
    case CXCursor_CXXDeleteExpr: return "CXXDeleteExpr"; // 135
    case CXCursor_UnaryExpr: return "UnaryExpr"; // 136
    case CXCursor_ObjCStringLiteral: return "ObjCStringLiteral"; // 137
    case CXCursor_ObjCEncodeExpr: return "ObjCEncodeExpr"; // 138
    case CXCursor_ObjCSelectorExpr: return "ObjCSelectorExpr"; // 139
    case CXCursor_ObjCProtocolExpr: return "ObjCProtocolExpr"; // 140
    case CXCursor_ObjCBridgedCastExpr: return "ObjCBridgedCastExpr"; // 141
    case CXCursor_PackExpansionExpr: return "PackExpansionExpr"; // 142
    case CXCursor_SizeOfPackExpr: return "SizeOfPackExpr"; // 143
    case CXCursor_LambdaExpr: return "LambdaExpr"; // 144
    case CXCursor_ObjCBoolLiteralExpr: return "ObjCBoolLiteralExpr"; // 145
    case CXCursor_ObjCSelfExpr: return "ObjCSelfExpr"; // 146
    case CXCursor_OMPArraySectionExpr: return "OMPArraySectionExpr"; // 147
    case CXCursor_ObjCAvailabilityCheckExpr: return "ObjCAvailabilityCheckExpr"; // 148
    //case CXCursor_FixedPointLiteral: return "FixedPointLiteral"; // 149
    case CXCursor_UnexposedStmt: return "UnexposedStmt"; // 200
    case CXCursor_LabelStmt: return "LabelStmt"; // 201
    case CXCursor_CompoundStmt: return "CompoundStmt"; // 202
    case CXCursor_CaseStmt: return "CaseStmt"; // 203
    case CXCursor_DefaultStmt: return "DefaultStmt"; // 204
    case CXCursor_IfStmt: return "IfStmt"; // 205
    case CXCursor_SwitchStmt: return "SwitchStmt"; // 206
    case CXCursor_WhileStmt: return "WhileStmt"; // 207
    case CXCursor_DoStmt: return "DoStmt"; // 208
    case CXCursor_ForStmt: return "ForStmt"; // 209
    case CXCursor_GotoStmt: return "GotoStmt"; // 210
    case CXCursor_IndirectGotoStmt: return "IndirectGotoStmt"; // 211
    case CXCursor_ContinueStmt: return "ContinueStmt"; // 212
    case CXCursor_BreakStmt: return "BreakStmt"; // 213
    case CXCursor_ReturnStmt: return "ReturnStmt"; // 214
    case CXCursor_GCCAsmStmt: return "GCCAsmStmt"; // 215
    case CXCursor_ObjCAtTryStmt: return "ObjCAtTryStmt"; // 216
    case CXCursor_ObjCAtCatchStmt: return "ObjCAtCatchStmt"; // 217
    case CXCursor_ObjCAtFinallyStmt: return "ObjCAtFinallyStmt"; // 218
    case CXCursor_ObjCAtThrowStmt: return "ObjCAtThrowStmt"; // 219
    case CXCursor_ObjCAtSynchronizedStmt: return "ObjCAtSynchronizedStmt"; // 220
    case CXCursor_ObjCAutoreleasePoolStmt: return "ObjCAutoreleasePoolStmt"; // 221
    case CXCursor_ObjCForCollectionStmt: return "ObjCForCollectionStmt"; // 222
    case CXCursor_CXXCatchStmt: return "CXXCatchStmt"; // 223
    case CXCursor_CXXTryStmt: return "CXXTryStmt"; // 224
    case CXCursor_CXXForRangeStmt: return "CXXForRangeStmt"; // 225
    case CXCursor_SEHTryStmt: return "SEHTryStmt"; // 226
    case CXCursor_SEHExceptStmt: return "SEHExceptStmt"; // 227
    case CXCursor_SEHFinallyStmt: return "SEHFinallyStmt"; // 228
    case CXCursor_MSAsmStmt: return "MSAsmStmt"; // 229
    case CXCursor_NullStmt: return "NullStmt"; // 230
    case CXCursor_DeclStmt: return "DeclStmt"; // 231
    case CXCursor_OMPParallelDirective: return "OMPParallelDirective"; // 232
    case CXCursor_OMPSimdDirective: return "OMPSimdDirective"; // 233
    case CXCursor_OMPForDirective: return "OMPForDirective"; // 234
    case CXCursor_OMPSectionsDirective: return "OMPSectionsDirective"; // 235
    case CXCursor_OMPSectionDirective: return "OMPSectionDirective"; // 236
    case CXCursor_OMPSingleDirective: return "OMPSingleDirective"; // 237
    case CXCursor_OMPParallelForDirective: return "OMPParallelForDirective"; // 238
    case CXCursor_OMPParallelSectionsDirective: return "OMPParallelSectionsDirective"; // 239
    case CXCursor_OMPTaskDirective: return "OMPTaskDirective"; // 240
    case CXCursor_OMPMasterDirective: return "OMPMasterDirective"; // 241
    case CXCursor_OMPCriticalDirective: return "OMPCriticalDirective"; // 242
    case CXCursor_OMPTaskyieldDirective: return "OMPTaskyieldDirective"; // 243
    case CXCursor_OMPBarrierDirective: return "OMPBarrierDirective"; // 244
    case CXCursor_OMPTaskwaitDirective: return "OMPTaskwaitDirective"; // 245
    case CXCursor_OMPFlushDirective: return "OMPFlushDirective"; // 246
    case CXCursor_SEHLeaveStmt: return "SEHLeaveStmt"; // 247
    case CXCursor_OMPOrderedDirective: return "OMPOrderedDirective"; // 248
    case CXCursor_OMPAtomicDirective: return "OMPAtomicDirective"; // 249
    case CXCursor_OMPForSimdDirective: return "OMPForSimdDirective"; // 250
    case CXCursor_OMPParallelForSimdDirective: return "OMPParallelForSimdDirective"; // 251
    case CXCursor_OMPTargetDirective: return "OMPTargetDirective"; // 252
    case CXCursor_OMPTeamsDirective: return "OMPTeamsDirective"; // 253
    case CXCursor_OMPTaskgroupDirective: return "OMPTaskgroupDirective"; // 254
    case CXCursor_OMPCancellationPointDirective: return "OMPCancellationPointDirective"; // 255
    case CXCursor_OMPCancelDirective: return "OMPCancelDirective"; // 256
    case CXCursor_OMPTargetDataDirective: return "OMPTargetDataDirective"; // 257
    case CXCursor_OMPTaskLoopDirective: return "OMPTaskLoopDirective"; // 258
    case CXCursor_OMPTaskLoopSimdDirective: return "OMPTaskLoopSimdDirective"; // 259
    case CXCursor_OMPDistributeDirective: return "OMPDistributeDirective"; // 260
    case CXCursor_OMPTargetEnterDataDirective: return "OMPTargetEnterDataDirective"; // 261
    case CXCursor_OMPTargetExitDataDirective: return "OMPTargetExitDataDirective"; // 262
    case CXCursor_OMPTargetParallelDirective: return "OMPTargetParallelDirective"; // 263
    case CXCursor_OMPTargetParallelForDirective: return "OMPTargetParallelForDirective"; // 264
    case CXCursor_OMPTargetUpdateDirective: return "OMPTargetUpdateDirective"; // 265
    case CXCursor_OMPDistributeParallelForDirective: return "OMPDistributeParallelForDirective"; // 266
    case CXCursor_OMPDistributeParallelForSimdDirective: return "OMPDistributeParallelForSimdDirective"; // 267
    case CXCursor_OMPDistributeSimdDirective: return "OMPDistributeSimdDirective"; // 268
    case CXCursor_OMPTargetParallelForSimdDirective: return "OMPTargetParallelForSimdDirective"; // 269
    case CXCursor_OMPTargetSimdDirective: return "OMPTargetSimdDirective"; // 270
    case CXCursor_OMPTeamsDistributeDirective: return "OMPTeamsDistributeDirective"; // 271
    case CXCursor_OMPTeamsDistributeSimdDirective: return "OMPTeamsDistributeSimdDirective"; // 272
    case CXCursor_OMPTeamsDistributeParallelForSimdDirective: return "OMPTeamsDistributeParallelForSimdDirective"; // 273
    case CXCursor_OMPTeamsDistributeParallelForDirective: return "OMPTeamsDistributeParallelForDirective"; // 274
    case CXCursor_OMPTargetTeamsDirective: return "OMPTargetTeamsDirective"; // 275
    case CXCursor_OMPTargetTeamsDistributeDirective: return "OMPTargetTeamsDistributeDirective"; // 276
    case CXCursor_OMPTargetTeamsDistributeParallelForDirective: return "OMPTargetTeamsDistributeParallelForDirective"; // 277
    case CXCursor_OMPTargetTeamsDistributeParallelForSimdDirective: return "OMPTargetTeamsDistributeParallelForSimdDirective"; // 278
    case CXCursor_OMPTargetTeamsDistributeSimdDirective: return "OMPTargetTeamsDistributeSimdDirective"; // 279
    case CXCursor_TranslationUnit: return "TranslationUnit"; // 300
    case CXCursor_UnexposedAttr: return "UnexposedAttr"; // 400
    case CXCursor_IBActionAttr: return "IBActionAttr"; // 401
    case CXCursor_IBOutletAttr: return "IBOutletAttr"; // 402
    case CXCursor_IBOutletCollectionAttr: return "IBOutletCollectionAttr"; // 403
    case CXCursor_CXXFinalAttr: return "CXXFinalAttr"; // 404
    case CXCursor_CXXOverrideAttr: return "CXXOverrideAttr"; // 405
    case CXCursor_AnnotateAttr: return "AnnotateAttr"; // 406
    case CXCursor_AsmLabelAttr: return "AsmLabelAttr"; // 407
    case CXCursor_PackedAttr: return "PackedAttr"; // 408
    case CXCursor_PureAttr: return "PureAttr"; // 409
    case CXCursor_ConstAttr: return "ConstAttr"; // 410
    case CXCursor_NoDuplicateAttr: return "NoDuplicateAttr"; // 411
    case CXCursor_CUDAConstantAttr: return "CUDAConstantAttr"; // 412
    case CXCursor_CUDADeviceAttr: return "CUDADeviceAttr"; // 413
    case CXCursor_CUDAGlobalAttr: return "CUDAGlobalAttr"; // 414
    case CXCursor_CUDAHostAttr: return "CUDAHostAttr"; // 415
    case CXCursor_CUDASharedAttr: return "CUDASharedAttr"; // 416
    case CXCursor_VisibilityAttr: return "VisibilityAttr"; // 417
    case CXCursor_DLLExport: return "DLLExport"; // 418
    case CXCursor_DLLImport: return "DLLImport"; // 419
    //case CXCursor_NSReturnsRetained: return "NSReturnsRetained"; // 420
    //case CXCursor_NSReturnsNotRetained: return "NSReturnsNotRetained"; // 421
    //case CXCursor_NSReturnsAutoreleased: return "NSReturnsAutoreleased"; // 422
    //case CXCursor_NSConsumesSelf: return "NSConsumesSelf"; // 423
    //case CXCursor_NSConsumed: return "NSConsumed"; // 424
    //case CXCursor_ObjCException: return "ObjCException"; // 425
    //case CXCursor_ObjCNSObject: return "ObjCNSObject"; // 426
    //case CXCursor_ObjCIndependentClass: return "ObjCIndependentClass"; // 427
    //case CXCursor_ObjCPreciseLifetime: return "ObjCPreciseLifetime"; // 428
    //case CXCursor_ObjCReturnsInnerPointer: return "ObjCReturnsInnerPointer"; // 429
    //case CXCursor_ObjCRequiresSuper: return "ObjCRequiresSuper"; // 430
    //case CXCursor_ObjCRootClass: return "ObjCRootClass"; // 431
    //case CXCursor_ObjCSubclassingRestricted: return "ObjCSubclassingRestricted"; // 432
    //case CXCursor_ObjCExplicitProtocolImpl: return "ObjCExplicitProtocolImpl"; // 433
    //case CXCursor_ObjCDesignatedInitializer: return "ObjCDesignatedInitializer"; // 434
    //case CXCursor_ObjCRuntimeVisible: return "ObjCRuntimeVisible"; // 435
    //case CXCursor_ObjCBoxable: return "ObjCBoxable"; // 436
    //case CXCursor_FlagEnum: return "FlagEnum"; // 437
    case CXCursor_PreprocessingDirective: return "PreprocessingDirective"; // 500
    case CXCursor_MacroDefinition: return "MacroDefinition"; // 501
    case CXCursor_MacroExpansion: return "MacroExpansion"; // 502
    case CXCursor_InclusionDirective: return "InclusionDirective"; // 503
    case CXCursor_ModuleImportDecl: return "ModuleImportDecl"; // 600
    case CXCursor_TypeAliasTemplateDecl: return "TypeAliasTemplateDecl"; // 601
    case CXCursor_StaticAssert: return "StaticAssert"; // 602
    case CXCursor_FriendDecl: return "FriendDecl"; // 603
    case CXCursor_OverloadCandidate: return "OverloadCandidate"; // 700
  }
  return std::to_string(kind);
}

std::string getTokenKindName(CXTokenKind kind) {    
  switch (kind) {
    case CXToken_Comment: return "Comment";
    case CXToken_Identifier: return "Identifier";
    case CXToken_Keyword: return "Keyword";
    case CXToken_Literal: return "Literal";
    case CXToken_Punctuation: return "Punctuation";
  }
  return std::to_string(kind);
}
} /* WhatsUpDoc */